#ifndef __LIGHT_H
#define __LIGHT_H

#include "sys.h"

/* Photoresistor analog output: ADC1 channel 7 (PA7). */
#define LIGHT_ADC_CHANNEL       ADC_CHANNEL_7
#define LIGHT_ADC_SAMPLE_TIMES  20

#define LIGHT_ADC_VREF          3.3f
#define LIGHT_ADC_RESOLUTION    4096u

/* External LED, active high. */
#define LIGHT_LED_GPIO_PORT     GPIOC
#define LIGHT_LED_GPIO_PIN      GPIO_PIN_3
#define LIGHT_LED_ACTIVE_HIGH   0

/* Light percent is inverted from ADC voltage: strong light gives low voltage. */
#define LIGHT_DARK_THRESHOLD    50u

void  LIGHT_Init(void);
void  LIGHT_SelfTest(void);
u16   LIGHT_Get_Raw_Value(void);
float LIGHT_Voltage_From_Raw(u16 raw);
float LIGHT_Get_Voltage(void);
u8    LIGHT_Brightness_From_Raw(u16 raw);
u8    LIGHT_Is_Dark_From_Raw(u16 raw);
void  LIGHT_LED_Set(u8 on);

#endif
