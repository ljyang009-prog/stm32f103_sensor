#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "usmart.h"
#include "mq2.h"
#include "dht11.h"
#include "buzzer.h"
#include "light.h"
#include "esp8266.h"
#include "onenet.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_tasks.h"

int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(RCC_PLL_MUL9);
    delay_init(72);
    uart_init(115200);
    usmart_dev.init(84);
    LED_Init();
    LCD_Init();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    MQ2_Init();
    DHT11_Init();
    BUZZER_Init();
    LIGHT_Init();
    LIGHT_SelfTest();

    POINT_COLOR = RED;
    LCD_ShowString(30, 50, 200, 16, 16, "Mini STM32");
    LCD_ShowString(30, 70, 200, 16, 16, "MQ-2 GAS TEST");
    LCD_ShowString(30, 90, 200, 16, 16, "ADC1_CH1/PA1");
    POINT_COLOR = BLUE;
    LCD_ShowString(30, 110, 200, 16, 16, "ONENET:MQTT");
    LCD_ShowString(30, 130, 200, 16, 16, "MQ2_RAW_VAL:");
    LCD_ShowString(30, 150, 200, 16, 16, "MQ2_PPM:");
    LCD_ShowString(30, 170, 200, 16, 16, "MQ2_STATUS:OK");
    LCD_ShowString(30, 190, 200, 16, 16, "DHT11_TEMP:");
    LCD_ShowString(30, 210, 200, 16, 16, "DHT11_HUMI:");
    LCD_ShowString(30, 230, 200, 16, 16, "TEMP_ALM:OK");
    LCD_ShowString(30, 250, 200, 16, 16, "HUMI_ALM:OK");
    LCD_ShowString(30, 270, 200, 16, 16, "LIGHT:   % OFF");

    if (AppTasks_Create() != 0u)
    {
        printf("FreeRTOS object creation failed\r\n");
        while (1)
        {
        }
    }

    vTaskStartScheduler();

    while (1)
    {
    }
}
