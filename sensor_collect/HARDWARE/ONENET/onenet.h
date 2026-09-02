#ifndef __ONENET_H
#define __ONENET_H

#include "sys.h"
#include "esp8266.h"

/* ESP8266 MQTT link id used by the AT+MQTT... commands. */
#define ONENET_MQTT_LINK    0

#define ONENET_LED_OFF      0
#define ONENET_LED_ON       1
#define ONENET_LED_BLINK    2

/* Alarm source bit mask. Multiple sources can be active at the same time. */
#define ONENET_ALARM_GAS            0x01
#define ONENET_ALARM_TEMP_LOW       0x02
#define ONENET_ALARM_TEMP_HIGH      0x04
#define ONENET_ALARM_HUMIDITY_LOW   0x08
#define ONENET_ALARM_HUMIDITY_HIGH  0x10

u8 OneNet_Init(void);
u8 OneNet_SendData(float temperature, float humidity,
                   float gas, float light, u8 alarm_mask);
u8 OneNet_SendAlarmEvent(u8 trigger_mask, float temperature,
                         float humidity, float gas);
u8 OneNet_Process(void);
u8 OneNet_GetLedMode(void);

#endif
