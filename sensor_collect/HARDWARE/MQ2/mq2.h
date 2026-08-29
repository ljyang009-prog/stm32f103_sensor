#ifndef __MQ2_H
#define __MQ2_H

#include "sys.h"

/* MQ-2 analog output: ADC1 channel 1 (PA1). */
#define MQ2_ADC_CHANNEL         ADC_CHANNEL_1
#define MQ2_ADC_SAMPLE_TIMES    20

/* ADC reference voltage and 12-bit resolution. */
#define MQ2_ADC_VREF            3.3f
#define MQ2_ADC_RESOLUTION      4096u

/* MQ-2 digital output: low level means gas leakage. */
#define MQ2_DOUT_GPIO_PORT      GPIOA
#define MQ2_DOUT_GPIO_PIN       GPIO_PIN_4

/* AOUT voltage above this value is treated as gas/smoke alarm. */
#define MQ2_LEAK_VOLTAGE_THRESHOLD  2.5f

/* ---- Concentration conversion parameters (tune to your module) ---- */
#define MQ2_RL_OHM          1000.0f   /* module load resistor RL (ohm), 1k/10k common */
#define MQ2_VC_VOLT         5.0f      /* module supply voltage (V) */
#define MQ2_RO_CLEAN_FACTOR 9.8f      /* Rs/R0 ratio in clean air */

/* ppm = A * (Rs/R0)^B curve-fit coefficients (uncalibrated approximation) */
#define MQ2_LPG_A           613.9f
#define MQ2_LPG_B           (-2.074f)
#define MQ2_SMOKE_A         611.7f
#define MQ2_SMOKE_B         (-2.2f)

void  MQ2_Init(void);
u16   MQ2_Get_Raw_Value(void);
float MQ2_Voltage_From_Raw(u16 raw);
float MQ2_Get_Voltage(void);
u8    MQ2_Is_Gas_Leak_From_Raw(u16 raw);
u8    MQ2_Is_Gas_Leak(void);
float MQ2_GetRs(u16 raw);
void  MQ2_CalibrateR0(void);
float MQ2_GetRatio(u16 raw);
float MQ2_GetPPM_LPG(u16 raw);
float MQ2_GetPPM_Smoke(u16 raw);

#endif
