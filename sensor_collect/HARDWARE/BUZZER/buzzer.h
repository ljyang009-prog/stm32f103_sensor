#ifndef __BUZZER_H
#define __BUZZER_H

#include "sys.h"

/* Active-low buzzer on PC1. */
#define BUZZER_GPIO_PORT    GPIOC
#define BUZZER_GPIO_PIN     GPIO_PIN_1

#define BUZZER_ON()         HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET)
#define BUZZER_OFF()        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET)

void BUZZER_Init(void);

#endif
