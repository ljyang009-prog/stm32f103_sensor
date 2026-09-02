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

static u8 ESP8266_WaitAny(u8 *ack1, u8 *ack2, u16 timeout_ms)
{
    u32 t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) <= timeout_ms)
    {
        if (ESP8266_FindStr(ack1, (u16)strlen((char *)ack1)) ||
            ESP8266_FindStr(ack2, (u16)strlen((char *)ack2)))
        {
            return ESP8266_OK;
        }
        if (ESP8266_FindStr((u8 *)"ERROR", 5) ||
            ESP8266_FindStr((u8 *)"FAIL", 4))
        {
            return ESP8266_ERR;
        }
        delay_ms(1);
    }
    return ESP8266_ERR;
}

static u8 ESP8266_SendCmdAny(u8 *cmd, u8 *ack1, u8 *ack2,
                             u16 timeout_ms)
{
    ESP8266_ClearRx();
    ESP8266_Uart_Send(cmd, (u16)strlen((char *)cmd));
    ESP8266_Uart_Send((u8 *)"\r\n", 2);
    return ESP8266_WaitAny(ack1, ack2, timeout_ms);
}

static void ESP8266_PrintRx(const char *label)
{
    u8 raw[96];
    u16 n;
    u16 i;

    n = ESP8266_ReadRx(raw, sizeof(raw));
    printf("%s", label);
    if (n == 0)
    {
        printf("<no response>\r\n");
        return;
    }

    for (i = 0; i < n; i++)
    {
        u8 c = raw[i];
        if (c >= 0x20 && c <= 0x7E)
            printf("%c", c);
        else if (c == '\r' || c == '\n')
            printf(" ");
        else
            printf(".");
    }
    printf("\r\n");
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
    u8 i;

    ESP8266_Uart_Init(ESP8266_UART_BAUD);

#if ESP8266_USE_HW_RESET
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_RESET);
    delay_ms(250);
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
    delay_ms(2000);                 /* ESP-AT needs ~1-2s to boot */
#else
    delay_ms(2000);
#endif

    ESP8266_ClearRx();

    /* Module present? retry a few times (ESP-AT boot is slow). */
    for (i = 0; i < 5; i++)
    {
        if (ESP8266_SendCmd((u8 *)"AT", (u8 *)"OK", 2000) == ESP8266_OK)
            break;
        delay_ms(500);
    }
    if (i == 5)
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

u8 ESP8266_ConnectSSL(u8 *host, u16 port)
{
    char cmd[112];

    /* OneNET's token is too long for AT+MQTTUSERCFG on ESP8266 ESP-AT.
     * Use the AT SSL socket and let the STM32 build the MQTT packets. */
    /* ESP-AT may preserve CIPMUX across resets. CIPMODE can only be changed
     * while multiplexing is disabled, so normalize the state first. */
    if (ESP8266_SendCmd((u8 *)"AT+CIPMUX=0", (u8 *)"OK", 2000) != ESP8266_OK)
    {
        ESP8266_PrintRx("ESP CIPMUX=0 response: ");
        return ESP8266_ERR;
    }

    /* Some older AT firmware does not implement CIPMODE. Non-transparent
     * mode is already the reset default, so this command is optional. */
    if (ESP8266_SendCmd((u8 *)"AT+CIPMODE=0", (u8 *)"OK", 1000) != ESP8266_OK)
    {
        ESP8266_PrintRx("ESP CIPMODE response: ");
    }

    if (ESP8266_SendCmd((u8 *)"AT+CIPMUX=1", (u8 *)"OK", 2000) != ESP8266_OK)
    {
        ESP8266_PrintRx("ESP CIPMUX response: ");
        return ESP8266_ERR;
    }

    /* Keep +IPD in the parser's expected +IPD,<link>,<len>: format.
     * Older AT firmware may not implement this command; its default is 0. */
    ESP8266_SendCmd((u8 *)"AT+CIPDINFO=0", (u8 *)"OK", 2000);

    sprintf(cmd, "AT+CIPSTART=0,\"SSL\",\"%s\",%d", (char *)host, port);
    if (ESP8266_SendCmdAny((u8 *)cmd, (u8 *)"OK", (u8 *)"CONNECT",
                           20000) != ESP8266_OK)
    {
        ESP8266_PrintRx("ESP SSL response: ");
        ESP8266_SendCmd((u8 *)"AT+GMR", (u8 *)"OK", 2000);
        ESP8266_PrintRx("ESP firmware: ");
        return ESP8266_ERR;
    }

    return ESP8266_OK;
}

u16 ESP8266_GetTcpData(u8 *buf, u16 maxlen)
{
    u16 total = ESP8266_GetRxLen();
    u16 i, j, n;
    u16 dlen = 0;
    u16 start;

    if (total < 8)
        return 0;

    /* Search for "+IPD,<link>,<len>:" anywhere in the buffer. */
    for (i = 0; (u16)(i + 5) <= total; i++)
    {
        if (ESP8266_Peek(i) == '+' && ESP8266_Peek(i + 1) == 'I' &&
            ESP8266_Peek(i + 2) == 'P' && ESP8266_Peek(i + 3) == 'D' &&
            ESP8266_Peek(i + 4) == ',')
        {
            /* length digits start at i+7 (i+5 = link id, i+6 = ','). */
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
            if (j >= total || ESP8266_Peek(j) != ':')
                return 0;                    /* header not fully received yet */

            j++;                             /* points at the payload */
            start = j;

            if ((u16)(total - start) < dlen)
                return 0;                    /* payload not all arrived yet */

            n = (dlen < maxlen) ? dlen : maxlen;
            for (j = 0; j < n; j++)
                buf[j] = ESP8266_Peek(start + j);

            /* Consume leading garbage + header + this payload only. */
            esp8266_dev.rx_tail = (u16)((esp8266_dev.rx_tail + start + dlen) & RX_MASK);
            return n;
        }
    }
    return 0;
}

u8 ESP8266_SendTcpData(u8 link_id, u8 *data, u16 len)
{
    char cmd[32];
    u8 r = ESP8266_OK;

    sprintf(cmd, "AT+CIPSEND=%d,%d", link_id, len);
    ESP8266_ClearRx();
    ESP8266_Uart_Send((u8 *)cmd, (u16)strlen(cmd));
    ESP8266_Uart_Send((u8 *)"\r\n", 2);

    /* Wait for the ">" prompt, then send the raw payload. */
    if (ESP8266_WaitAck((u8 *)">", 2000) != ESP8266_OK)
    {
        ESP8266_PrintRx("ESP CIPSEND prompt response: ");
        return ESP8266_ERR;
    }

    ESP8266_ClearRx();              /* drop the "OK\r\n>" residue */
    ESP8266_Uart_Send(data, len);

    /* Some firmware says "SEND OK", older ones just "OK". Accept both and
     * stop immediately on SEND FAIL/ERROR so reconnect starts promptly. */
    if (ESP8266_WaitAny((u8 *)"SEND OK", (u8 *)"\r\nOK\r\n", 5000) != ESP8266_OK)
    {
        ESP8266_PrintRx("ESP payload send response: ");
        r = ESP8266_ERR;
    }

    return r;
}

u8 ESP8266_CloseLink(u8 link_id)
{
    char cmd[32];
    sprintf(cmd, "AT+CIPCLOSE=%d", link_id);
    return ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 2000);
}

/*------------------------------------------------------------------------------
 * MQTT client (ESP-AT firmware: AT+MQTT...)
 *----------------------------------------------------------------------------*/
u8 ESP8266_MQTTUserCfg(u8 link_id, char *client_id, char *user, char *pass)
{
    char cmd[360];

    /* scheme=1 : MQTT over TCP. cert/CA/path are not used. */
    sprintf(cmd, "AT+MQTTUSERCFG=%d,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
            link_id, client_id, user, pass);
    return ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 3000);
}

u8 ESP8266_MQTTConn(u8 link_id, char *host, u16 port, u8 reconnect)
{
    char cmd[160];

    sprintf(cmd, "AT+MQTTCONN=%d,\"%s\",%d,%d", link_id, host, port, reconnect);
    if (ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 5000) != ESP8266_OK)
        return ESP8266_ERR;

    /* Result arrives asynchronously as "+MQTTCONNECTED:<link>,...".
     * (The buffer still holds the response, so search it directly.) */
    return ESP8266_WaitAck((u8 *)"MQTTCONNECTED", 8000);
}

u8 ESP8266_MQTTPub(u8 link_id, char *topic, char *data, u8 qos, u8 retain)
{
    char cmd[640];
    int i, j;

    /* Escape '"' and '\' so JSON payloads survive the AT quoted-string parser. */
    j = sprintf(cmd, "AT+MQTTPUB=%d,\"%s\",\"", link_id, topic);
    for (i = 0; data[i] != '\0' && j < (int)sizeof(cmd) - 4; i++)
    {
        if (data[i] == '"' || data[i] == '\\')
            cmd[j++] = '\\';
        cmd[j++] = data[i];
    }
    j += sprintf(cmd + j, "\",%d,%d", qos, retain);

    return ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 5000);
}

u8 ESP8266_MQTTPubRaw(u8 link_id, char *topic, u8 *data, u16 len, u8 qos, u8 retain)
{
    char cmd[128];

    sprintf(cmd, "AT+MQTTPUBRAW=%d,\"%s\",%d,%d,%d", link_id, topic, len, qos, retain);
    ESP8266_ClearRx();
    ESP8266_Uart_Send((u8 *)cmd, (u16)strlen(cmd));
    ESP8266_Uart_Send((u8 *)"\r\n", 2);

    /* Firmware replies "OK\r\n>" then accepts <len> raw bytes (no escaping). */
    if (ESP8266_WaitAck((u8 *)">", 3000) != ESP8266_OK)
        return ESP8266_ERR;

    ESP8266_ClearRx();              /* drop the "OK\r\n>" residue */
    ESP8266_Uart_Send(data, len);

    return ESP8266_WaitAck((u8 *)"OK", 5000);
}

u8 ESP8266_MQTTClean(u8 link_id)
{
    char cmd[32];
    sprintf(cmd, "AT+MQTTCLEAN=%d", link_id);
    return ESP8266_SendCmd((u8 *)cmd, (u8 *)"OK", 2000);
}
