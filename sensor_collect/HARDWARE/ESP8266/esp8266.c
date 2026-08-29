/**
 * ESP8266 WiFi module driver (AT command set).
 *
 * UART  : USART2 (PA2 = TX, PA3 = RX)
 * Reset : PA0 (optional, active low)
 *
 * The module runs the standard ESP8266 AT firmware and talks at 115200 baud.
 */

#include "esp8266.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

ESP8266_Dev esp8266_dev;

#define RX_MASK  ((u16)(ESP8266_RX_BUF_SIZE - 1))

/*------------------------------------------------------------------------------
 * Ring buffer
 *----------------------------------------------------------------------------*/
void ESP8266_ClearRx(void)
{
    esp8266_dev.rx_head = 0;
    esp8266_dev.rx_tail = 0;
}

u16 ESP8266_GetRxLen(void)
{
    return (u16)((esp8266_dev.rx_head - esp8266_dev.rx_tail) & RX_MASK);
}

u16 ESP8266_ReadRx(u8 *buf, u16 len)
{
    u16 i;
    u16 n = ESP8266_GetRxLen();

    if (n > len)
        n = len;
    for (i = 0; i < n; i++)
    {
        buf[i] = esp8266_dev.rx_buf[(esp8266_dev.rx_tail + i) & RX_MASK];
    }
    esp8266_dev.rx_tail = (u16)((esp8266_dev.rx_tail + n) & RX_MASK);
    return n;
}

static void ESP8266_PushRx(u8 c)
{
    u16 next = (u16)((esp8266_dev.rx_head + 1) & RX_MASK);

    if (next != esp8266_dev.rx_tail)          /* not full */
    {
        esp8266_dev.rx_buf[esp8266_dev.rx_head] = c;
        esp8266_dev.rx_head = next;
    }
}

static u8 ESP8266_Peek(u16 offset)
{
    return esp8266_dev.rx_buf[(esp8266_dev.rx_tail + offset) & RX_MASK];
}

/*------------------------------------------------------------------------------
 * UART2 low level
 *----------------------------------------------------------------------------*/
void ESP8266_Uart_Init(u32 bound)
{
    GPIO_InitTypeDef gpio;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* PA2 = USART2_TX */
    gpio.Pin   = GPIO_PIN_2;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PA3 = USART2_RX */
    gpio.Pin  = GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

#if ESP8266_USE_HW_RESET
    /* ESP8266 RST pin (active low) */
    gpio.Pin   = ESP8266_RST_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ESP8266_RST_PORT, &gpio);
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
#endif

    /* 8N1, TX + RX */
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = 0;
    USART2->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK1Freq(), bound);
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* Enable RXNE interrupt */
    USART2->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 3);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void ESP8266_Uart_Send(u8 *buf, u16 len)
{
    while (len--)
    {
        while (!(USART2->SR & USART_SR_TXE))
        {
        }
        USART2->DR = *buf++;
    }
}

/* Send a null-terminated string over USART2 (to the ESP8266). */
void ESP8266_Uart_SendStr(u8 *str)
{
    ESP8266_Uart_Send(str, (u16)strlen((char *)str));
}

void USART2_IRQHandler(void)
{
    if (USART2->SR & USART_SR_RXNE)
    {
        ESP8266_PushRx((u8)(USART2->DR));
    }
    if (USART2->SR & USART_SR_ORE)
    {
        (void)USART2->SR;   /* read SR then DR to clear ORE */
        (void)USART2->DR;
    }
}

/*------------------------------------------------------------------------------
 * AT command helpers
 *----------------------------------------------------------------------------*/
static u8 ESP8266_FindStr(u8 *str, u16 slen)
{
    u16 len = ESP8266_GetRxLen();
    u16 i, j;

    if (slen == 0 || len < slen)
        return 0;

    for (i = 0; i <= (u16)(len - slen); i++)
    {
        u8 ok = 1;
        for (j = 0; j < slen; j++)
        {
            if (esp8266_dev.rx_buf[(esp8266_dev.rx_tail + i + j) & RX_MASK] != str[j])
            {
                ok = 0;
                break;
            }
        }
        if (ok)
            return 1;
    }
    return 0;
}

static u8 ESP8266_WaitAck(u8 *ack, u16 timeout_ms)
{
    u32 t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) <= timeout_ms)
    {
        if (ESP8266_FindStr(ack, (u16)strlen((char *)ack)))
            return ESP8266_OK;
        delay_ms(1);
    }
    return ESP8266_ERR;
}

u8 ESP8266_SendCmd(u8 *cmd, u8 *ack, u16 timeout_ms)
{
    ESP8266_ClearRx();
    ESP8266_Uart_Send(cmd, (u16)strlen((char *)cmd));
    ESP8266_Uart_Send((u8 *)"\r\n", 2);

    if (ack == NULL)
        return ESP8266_OK;      /* fire and forget */

    return ESP8266_WaitAck(ack, timeout_ms);
}

/*------------------------------------------------------------------------------
 * High level
 *----------------------------------------------------------------------------*/
u8 ESP8266_Init(void)
{
    ESP8266_Uart_Init(ESP8266_UART_BAUD);

#if ESP8266_USE_HW_RESET
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_RESET);
    delay_ms(250);
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
    delay_ms(500);
#else
    delay_ms(200);
#endif

    ESP8266_ClearRx();

    /* Module present? */
    if (ESP8266_SendCmd((u8 *)"AT", (u8 *)"OK", 2000) != ESP8266_OK)
        return ESP8266_ERR;

    /* Disable echo so responses are clean. */
    ESP8266_SendCmd((u8 *)"ATE0", (u8 *)"OK", 1000);

    /* Station mode. */
    if (ESP8266_SendCmd((u8 *)"AT+CWMODE=1", (u8 *)"OK", 2000) != ESP8266_OK)
        return ESP8266_ERR;

    return ESP8266_OK;
}

u8 ESP8266_ConnectWifi(u8 *ssid, u8 *pwd)
{
    char cmd[128];

    ESP8266_SendCmd((u8 *)"AT+CWMODE=1", (u8 *)"OK", 2000);

    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", (char *)ssid, (char *)pwd);
    if (ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 15000) != ESP8266_OK)
        return ESP8266_ERR;
    return ESP8266_OK;
}

u8 ESP8266_GetIP(u8 *ipbuf)
{
    char resp[160];
    char *p;
    char *q;
    u16 n;
    int len;

    if (ESP8266_SendCmd((u8 *)"AT+CIFSR", (u8 *)"OK", 3000) != ESP8266_OK)
        return ESP8266_ERR;

    n = ESP8266_ReadRx((u8 *)resp, (u16)(sizeof(resp) - 1));
    resp[n] = '\0';

    /* Response: +CIFSR:STAIP,"192.168.1.100" ... */
    p = strstr(resp, "STAIP,");
    if (p == NULL)
        return ESP8266_ERR;
    p = strchr(p, '"');
    if (p == NULL)
        return ESP8266_ERR;
    p++;
    q = strchr(p, '"');
    if (q == NULL)
        return ESP8266_ERR;

    len = (int)(q - p);
    if (len > 15)
        len = 15;
    memcpy(ipbuf, p, (size_t)len);
    ipbuf[len] = '\0';
    return ESP8266_OK;
}

u8 ESP8266_CheckWifi(void)
{
    u8 ip[16];
    return ESP8266_GetIP(ip);
}

/*------------------------------------------------------------------------------
 * TCP client (MQTT / OneNet)
 *----------------------------------------------------------------------------*/
u8 ESP8266_ConnectTCP(u8 *ip, u16 port)
{
    char cmd[96];

    /* Multiple connections (use link 0). */
    if (ESP8266_SendCmd((u8 *)"AT+CIPMUX=1", (u8 *)"OK", 2000) != ESP8266_OK)
        return ESP8266_ERR;

    sprintf(cmd, "AT+CIPSTART=0,\"TCP\",\"%s\",%d", (char *)ip, port);
    if (ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 10000) != ESP8266_OK)
        return ESP8266_ERR;

    return ESP8266_OK;
}

u16 ESP8266_GetTcpData(u8 *buf, u16 maxlen)
{
    u16 total = ESP8266_GetRxLen();
    u16 i, j, n;
    u16 dlen = 0;

    if (total < 10)
        return 0;

    /* Search for "+IPD,<link>,<len>:" */
    for (i = 0; i <= (u16)(total - 5); i++)
    {
        if (ESP8266_Peek(i) == '+' && ESP8266_Peek(i + 1) == 'I' &&
            ESP8266_Peek(i + 2) == 'P' && ESP8266_Peek(i + 3) == 'D' &&
            ESP8266_Peek(i + 4) == ',')
        {
            if (i + 7 >= total)
                break;

            /* link id at i+5, ',' at i+6, length digits from i+7. */
            j = i + 7;
            dlen = 0;
            while (j < total)
            {
                u8 c = ESP8266_Peek(j);
                if (c < '0' || c > '9')
                    break;
                dlen = (u16)(dlen * 10 + (c - '0'));
                j++;
            }

            if (j < total && ESP8266_Peek(j) == ':')
            {
                j++;            /* points at the data */
                n = 0;
                while (j < total && n < dlen && n < maxlen)
                {
                    buf[n++] = ESP8266_Peek(j);
                    j++;
                }
                ESP8266_ClearRx();   /* consume this notification */
                return n;
            }
            break;
        }
    }
    return 0;
}

u8 ESP8266_SendTcpData(u8 link_id, u8 *data, u16 len)
{
    char cmd[32];

    sprintf(cmd, "AT+CIPSEND=%d,%d", link_id, len);
    ESP8266_ClearRx();
    ESP8266_Uart_Send((u8 *)cmd, (u16)strlen(cmd));
    ESP8266_Uart_Send((u8 *)"\r\n", 2);

    /* Wait for the ">" prompt, then send the raw payload. */
    if (ESP8266_WaitAck((u8 *)">", 2000) != ESP8266_OK)
        return ESP8266_ERR;

    ESP8266_Uart_Send(data, len);

    if (ESP8266_WaitAck((u8 *)"SEND OK", 3000) != ESP8266_OK)
        return ESP8266_ERR;

    return ESP8266_OK;
}

u8 ESP8266_CloseLink(u8 link_id)
{
    char cmd[32];
    sprintf(cmd, "AT+CIPCLOSE=%d", link_id);
    return ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 2000);
}
