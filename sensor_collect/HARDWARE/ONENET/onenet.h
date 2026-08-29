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

/* MQTT server (your product's domain from the console). */
#define ONENET_SERVER_IP    "D155yb7UYS.mqtts.acc.cmcconenet.cn"
#define ONENET_SERVER_PORT  1883

u8 OneNet_Init(void);
u8 OneNet_SendData(float temperature, float humidity,
                   float gas, float light);

#endif
