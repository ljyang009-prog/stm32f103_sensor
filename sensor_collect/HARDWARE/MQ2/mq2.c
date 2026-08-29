#include "mq2.h"
#include "adc.h"
#include <math.h>

static float mq2_r0 = 1000.0f;   /* R0; calibrate with MQ2_CalibrateR0() in clean air */

void MQ2_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    MY_ADC_Init();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_Initure.Pin = MQ2_DOUT_GPIO_PIN;
    GPIO_Initure.Mode = GPIO_MODE_INPUT;
    GPIO_Initure.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MQ2_DOUT_GPIO_PORT, &GPIO_Initure);
}

u16 MQ2_Get_Raw_Value(void)
{
    return Get_Adc_Average(MQ2_ADC_CHANNEL, MQ2_ADC_SAMPLE_TIMES);
}

float MQ2_Voltage_From_Raw(u16 raw)
{
    return ((float)raw * MQ2_ADC_VREF) / (float)MQ2_ADC_RESOLUTION;
}

float MQ2_Get_Voltage(void)
{
    return MQ2_Voltage_From_Raw(MQ2_Get_Raw_Value());
}

u8 MQ2_Is_Gas_Leak_From_Raw(u16 raw)
{
    if (HAL_GPIO_ReadPin(MQ2_DOUT_GPIO_PORT, MQ2_DOUT_GPIO_PIN) == GPIO_PIN_RESET)
    {
        return 1;
    }

    return (MQ2_Voltage_From_Raw(raw) >= MQ2_LEAK_VOLTAGE_THRESHOLD) ? 1 : 0;
}

u8 MQ2_Is_Gas_Leak(void)
{
    return MQ2_Is_Gas_Leak_From_Raw(MQ2_Get_Raw_Value());
}

/*------------------------------------------------------------------------------
 * Concentration conversion
 *----------------------------------------------------------------------------*/
/* Sensor resistance Rs (ohm): Rs = RL * (Vc - Vout) / Vout */
float MQ2_GetRs(u16 raw)
{
    float vout = MQ2_Voltage_From_Raw(raw);
    if (vout < 0.01f)
        vout = 0.01f;                 /* avoid divide by zero */
    return MQ2_RL_OHM * (MQ2_VC_VOLT - vout) / vout;
}

/* Calibrate R0 in clean air (call once in a gas-free environment). */
void MQ2_CalibrateR0(void)
{
    mq2_r0 = MQ2_GetRs(MQ2_Get_Raw_Value()) / MQ2_RO_CLEAN_FACTOR;
}

float MQ2_GetRatio(u16 raw)
{
    return MQ2_GetRs(raw) / mq2_r0;
}

float MQ2_GetPPM_LPG(u16 raw)
{
    float ratio = MQ2_GetRatio(raw);
    if (ratio < 0.001f)
        ratio = 0.001f;
    return MQ2_LPG_A * powf(ratio, MQ2_LPG_B);
}

float MQ2_GetPPM_Smoke(u16 raw)
{
    float ratio = MQ2_GetRatio(raw);
    if (ratio < 0.001f)
        ratio = 0.001f;
    return MQ2_SMOKE_A * powf(ratio, MQ2_SMOKE_B);
}
