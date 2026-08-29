#ifndef __DHT11_H
#define __DHT11_H

#include "sys.h"

/* DHT11 data pin (single wire). Change here to match your wiring. */
#define DHT11_DQ_GPIO_PORT      GPIOA
#define DHT11_DQ_GPIO_PIN       GPIO_PIN_5

#define DHT11_DQ_IN()           HAL_GPIO_ReadPin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN)
#define DHT11_DQ_OUT(x)         HAL_GPIO_WritePin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN, \
                                (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

/* Temperature/humidity alarm thresholds. Adjust to your needs. */
#define DHT11_TEMP_ALARM_LOW    10
#define DHT11_TEMP_ALARM_HIGH   35
#define DHT11_HUMI_ALARM_LOW    20
#define DHT11_HUMI_ALARM_HIGH   85

u8   DHT11_Init(void);
u8   DHT11_Read_Data(u8 *temp, u8 *humi);
u8   DHT11_Read_Byte(void);
u8   DHT11_Read_Bit(void);
u8   DHT11_Check(void);
void DHT11_Rst(void);

#endif
