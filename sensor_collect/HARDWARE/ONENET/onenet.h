#ifndef __ONENET_H
#define __ONENET_H

#include "sys.h"
#include "esp8266.h"

/*==============================================================================
 *  OneNET Studio Thing Model configuration (token auth / 一型一密)
 *============================================================================*/
#define ONENET_PROID        "D155yb7UYS"    /* product ID */
#define ONENET_DEVID        "sensor"        /* device name */

/* MQTT password = token (et must be a future unix timestamp). */
#define ONENET_TOKEN        "version=2018-10-31&res=products%2FD155yb7UYS%2Fdevices%2Fsensor&et=1819377258&method=md5&sign=BIjuVa1khfEcMI%2F88Cm5IQ%3D%3D"

/* OneNET Studio product-specific MQTT over TLS endpoint. */
#define ONENET_SERVER_HOST  "D155yb7UYS.mqttstls.acc.cmcconenet.cn"
#define ONENET_SERVER_PORT  8883

/* ESP8266 MQTT link id used by the AT+MQTT... commands. */
#define ONENET_MQTT_LINK    0

#define ONENET_LED_OFF      0
#define ONENET_LED_ON       1
#define ONENET_LED_BLINK    2

u8 OneNet_Init(void);
u8 OneNet_SendData(float temperature, float humidity,
                   float gas, float light);
u8 OneNet_Process(void);
u8 OneNet_GetLedMode(void);

#endif
