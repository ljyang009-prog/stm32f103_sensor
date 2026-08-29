#include "light.h"
#include "adc.h"
#include "delay.h"

#if LIGHT_LED_ACTIVE_HIGH
#define LIGHT_LED_ON_LEVEL      GPIO_PIN_SET
#define LIGHT_LED_OFF_LEVEL     GPIO_PIN_RESET
#else
#define LIGHT_LED_ON_LEVEL      GPIO_PIN_RESET
#define LIGHT_LED_OFF_LEVEL     GPIO_PIN_SET
#endif

void LIGHT_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_Initure.Pin = LIGHT_LED_GPIO_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LIGHT_LED_GPIO_PORT, &GPIO_Initure);

    LIGHT_LED_Set(0);
}

void LIGHT_SelfTest(void)
{
    u8 i;

    for (i = 0; i < 3; i++)
    {
        LIGHT_LED_Set(1);
        delay_ms(200);
        LIGHT_LED_Set(0);
        delay_ms(200);
    }
}

u16 LIGHT_Get_Raw_Value(void)
{
    return Get_Adc_Average(LIGHT_ADC_CHANNEL, LIGHT_ADC_SAMPLE_TIMES);
}

float LIGHT_Voltage_From_Raw(u16 raw)
{
    return ((float)raw * LIGHT_ADC_VREF) / (float)LIGHT_ADC_RESOLUTION;
}

float LIGHT_Get_Voltage(void)
{
    return LIGHT_Voltage_From_Raw(LIGHT_Get_Raw_Value());
}

u8 LIGHT_Brightness_From_Raw(u16 raw)
{
    u16 percent;

    percent = ((u32)((LIGHT_ADC_RESOLUTION - 1u) - raw) * 100u) / LIGHT_ADC_RESOLUTION;
    if (percent > 100u)
    {
        percent = 100u;
    }
    return (u8)percent;
}

u8 LIGHT_Is_Dark_From_Raw(u16 raw)
{
    return (LIGHT_Brightness_From_Raw(raw) < LIGHT_DARK_THRESHOLD) ? 1 : 0;
}

void LIGHT_LED_Set(u8 on)
{
    HAL_GPIO_WritePin(LIGHT_LED_GPIO_PORT, LIGHT_LED_GPIO_PIN,
                      on ? LIGHT_LED_ON_LEVEL : LIGHT_LED_OFF_LEVEL);
}
