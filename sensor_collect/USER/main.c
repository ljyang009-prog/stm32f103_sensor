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

int main(void)
{
    u16 adcx;
    float gas_ppm;
    u8 leak;
    u8 temp;
    u8 humi;
    u8 dht11_ok;
    u8 dht11_count;
    u8 temp_alarm;
    u8 humi_alarm;
    u8 alarm;
    u16 light_raw;
    u8 light_brightness;
    u8 light_dark;
    char dht_temp_str[24];
    char dht_humi_str[24];
    char light_str[24];
    u32 last_upload;
    char mqtt_line[64];

    HAL_Init();
    Stm32_Clock_Init(RCC_PLL_MUL9);
    delay_init(72);
    uart_init(115200);
    usmart_dev.init(84);
    LED_Init();
    LCD_Init();
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

    /* ESP8266 (Arduino firmware) handles WiFi + MQTT by itself.
     * STM32 only sends sensor data over USART2 (PA2). */
    printf("ESP8266 UART init...\r\n");
    ESP8266_Uart_Init(115200);

    /* Calibrate R0 after the sensor has warmed up a few seconds.
     * For accurate ppm, power on in clean (gas-free) air. */
    printf("MQ2 calibrate R0...\r\n");
    MQ2_CalibrateR0();
    printf("MQ2 calibrate R0 done\r\n");

    last_upload = HAL_GetTick();

    while (1)
    {
        adcx = MQ2_Get_Raw_Value();
        gas_ppm = MQ2_GetPPM_LPG(adcx);
        leak = MQ2_Is_Gas_Leak_From_Raw(adcx);

        LCD_ShowxNum(126, 130, adcx, 4, 16, 0);
        LCD_ShowxNum(94, 150, (u16)gas_ppm, 5, 16, 0);

        if (dht11_count == 0)
        {
            dht11_ok = (DHT11_Read_Data(&temp, &humi) == 0) ? 1 : 0;
        }
        dht11_count++;
        if (dht11_count >= 4)
        {
            dht11_count = 0;
        }

        light_raw = LIGHT_Get_Raw_Value();
        light_brightness = LIGHT_Brightness_From_Raw(light_raw);
        light_dark = LIGHT_Is_Dark_From_Raw(light_raw);
        LIGHT_LED_Set(light_dark);

        sprintf(light_str, "LIGHT:%3d%% %s", light_brightness, light_dark ? "ON" : "OFF");
        LCD_ShowString(30, 270, 200, 16, 16, (u8 *)light_str);

        if (dht11_ok == 1)
        {
            sprintf(dht_temp_str, "DHT11_TEMP:%dC", temp);
            sprintf(dht_humi_str, "DHT11_HUMI:%d%%", humi);
            temp_alarm = (temp > DHT11_TEMP_ALARM_HIGH || temp < DHT11_TEMP_ALARM_LOW) ? 1 : 0;
            humi_alarm = (humi > DHT11_HUMI_ALARM_HIGH || humi < DHT11_HUMI_ALARM_LOW) ? 1 : 0;
        }
        else
        {
            sprintf(dht_temp_str, "DHT11_TEMP:ERR");
            sprintf(dht_humi_str, "DHT11_HUMI:ERR");
            temp_alarm = 0;
            humi_alarm = 0;
        }
        LCD_ShowString(30, 190, 200, 16, 16, (u8 *)dht_temp_str);
        LCD_ShowString(30, 210, 200, 16, 16, (u8 *)dht_humi_str);

        if (dht11_ok == 1)
        {
            POINT_COLOR = temp_alarm ? RED : GREEN;
            LCD_ShowString(30, 230, 200, 16, 16, (u8 *)(temp_alarm ? "TEMP_ALM:ALM" : "TEMP_ALM:OK "));
            POINT_COLOR = humi_alarm ? RED : GREEN;
            LCD_ShowString(30, 250, 200, 16, 16, (u8 *)(humi_alarm ? "HUMI_ALM:ALM" : "HUMI_ALM:OK "));
            POINT_COLOR = BLUE;
        }
        else
        {
            POINT_COLOR = RED;
            LCD_ShowString(30, 230, 200, 16, 16, "TEMP_ALM:ERR");
            LCD_ShowString(30, 250, 200, 16, 16, "HUMI_ALM:ERR");
            POINT_COLOR = BLUE;
        }

        alarm = leak;
        alarm |= (temp_alarm | humi_alarm);
        if (alarm == 1)
        {
            BUZZER_ON();
        }
        else
        {
            BUZZER_OFF();
        }

        if (leak == 1)
        {
            LCD_ShowString(118, 170, 200, 16, 16, "LEAK!");
            printf("Gas leakage!!! ppm=%.1f\r\n", gas_ppm);
        }
        else
        {
            LCD_ShowString(118, 170, 200, 16, 16, "OK   ");
            printf("Gas not leakage!!! ppm=%.1f\r\n", gas_ppm);
        }

        if (dht11_ok == 1)
        {
            printf("DHT11: temp=%dC humi=%d%% temp_alarm=%d humi_alarm=%d\r\n",
                   temp, humi, temp_alarm, humi_alarm);
        }
        else
        {
            printf("DHT11: read error\r\n");
        }

        printf("LIGHT: raw=%d level=%d%% dark=%d\r\n", light_raw, light_brightness, light_dark);

        if ((HAL_GetTick() - last_upload) >= 5000)
        {
            last_upload = HAL_GetTick();
            sprintf(mqtt_line, "%d,%d,%.1f,%d\r\n", temp, humi, gas_ppm, light_brightness);
            ESP8266_Uart_SendStr((u8 *)mqtt_line);
            printf("Send to ESP8266: %s", mqtt_line);
        }

        LED0 = !LED0;
        delay_ms(250);
    }
}
