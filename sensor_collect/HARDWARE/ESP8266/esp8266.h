#ifndef __ESP8266_H
#define __ESP8266_H

#include "sys.h"

/*==============================================================================
 *  User configuration
 *============================================================================*/

/* WiFi access point to join (change these to your router). */
#define ESP8266_WIFI_SSID       "OnePlus 11"
#define ESP8266_WIFI_PASSWORD   "ljy050724"

/* Baud rate for the ESP8266 (default AT firmware runs at 115200). */
#define ESP8266_UART_BAUD       115200

/* Optional hardware reset line (active low).
 * Set ESP8266_USE_HW_RESET to 0 when the module RST pin is not connected. */
#define ESP8266_USE_HW_RESET    1
#define ESP8266_RST_PORT        GPIOA
#define ESP8266_RST_PIN         GPIO_PIN_0

/*============================================================================*/

#define ESP8266_RX_BUF_SIZE     1024        /* RX ring buffer size */

#define ESP8266_OK              0
#define ESP8266_ERR             1

typedef struct
{
    u8           rx_buf[ESP8266_RX_BUF_SIZE];
    volatile u16 rx_head;   /* write index */
    volatile u16 rx_tail;   /* read index */
} ESP8266_Dev;

extern ESP8266_Dev esp8266_dev;

/* UART / ring buffer */
void ESP8266_Uart_Init(u32 bound);
void ESP8266_Uart_SendStr(u8 *str);   /* send a null-terminated string */
void ESP8266_ClearRx(void);
u16  ESP8266_GetRxLen(void);
u16  ESP8266_ReadRx(u8 *buf, u16 len);

/* AT commands */
u8 ESP8266_SendCmd(u8 *cmd, u8 *ack, u16 timeout_ms);   /* send cmd + CRLF, wait for ack */
u8 ESP8266_Init(void);                                  /* detect module, set station mode */
u8 ESP8266_ConnectWifi(u8 *ssid, u8 *pwd);              /* join an access point */
u8 ESP8266_CheckWifi(void);                             /* check whether joined */
u8 ESP8266_GetIP(u8 *ipbuf);                            /* local IP, buf >= 16 bytes */

/* TCP client (MQTT / OneNet) */
u8  ESP8266_ConnectTCP(u8 *ip, u16 port);               /* CIPSTART TCP, link 0 */
u8  ESP8266_ConnectSSL(u8 *host, u16 port);             /* CIPSTART SSL, link 0 */
u16 ESP8266_GetTcpData(u8 *buf, u16 maxlen);            /* read one "+IPD" payload */
u8  ESP8266_SendTcpData(u8 link_id, u8 *data, u16 len); /* raw send on a link */
u8  ESP8266_CloseLink(u8 link_id);                      /* close a link */

/* MQTT client (ESP-AT firmware: AT+MQTT...) */
u8 ESP8266_MQTTUserCfg(u8 link_id, char *client_id, char *user, char *pass);
u8 ESP8266_MQTTConn(u8 link_id, char *host, u16 port, u8 reconnect);
u8 ESP8266_MQTTPub(u8 link_id, char *topic, char *data, u8 qos, u8 retain);
u8 ESP8266_MQTTPubRaw(u8 link_id, char *topic, u8 *data, u16 len, u8 qos, u8 retain);
u8 ESP8266_MQTTClean(u8 link_id);

#endif
