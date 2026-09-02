#include "app_tasks.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "buzzer.h"
#include "dht11.h"
#include "esp8266.h"
#include "lcd.h"
#include "led.h"
#include "light.h"
#include "mq2.h"
#include "onenet.h"

#define SENSOR_TASK_PERIOD_MS       1000u
#define DHT11_SAMPLE_PERIODS        2u
#define CLOUD_UPLOAD_PERIOD_MS      5000u
#define CLOUD_RETRY_PERIOD_MS       5000u

#define SENSOR_TASK_PRIORITY        4u
#define DISPLAY_TASK_PRIORITY       3u
#define CLOUD_TASK_PRIORITY         2u

#define SENSOR_TASK_STACK_WORDS     320u
#define CLOUD_TASK_STACK_WORDS      1024u
#define DISPLAY_TASK_STACK_WORDS    320u

typedef struct
{
    u16 gas_raw;
    float gas_ppm;
    u16 light_raw;
    u8 light_brightness;
    u8 light_dark;
    u8 temperature;
    u8 humidity;
    u8 dht11_valid;
    u8 gas_leak;
    u8 temp_alarm;
    u8 humidity_alarm;
    u8 alarm_mask;
} SensorSnapshot;

static QueueHandle_t sensor_to_cloud_queue;
static QueueHandle_t sensor_to_display_queue;

/* Runtime diagnostics readable from a debugger without stopping normal I/O. */
volatile u32 app_sensor_sample_count;
volatile u32 app_cloud_connect_count;
volatile u32 app_cloud_sample_count;
volatile u32 app_cloud_upload_ok_count;
volatile u32 app_cloud_upload_fail_count;

static void SensorTask(void *argument);
static void CloudTask(void *argument);
static void DisplayTask(void *argument);

static void LED0_Update(void)
{
    static u8 previous_mode = 0xFF;
    static TickType_t last_toggle = 0;
    TickType_t now;
    u8 mode;

    mode = OneNet_GetLedMode();
    now = xTaskGetTickCount();

    if (mode != previous_mode)
    {
        previous_mode = mode;
        last_toggle = now;
        LED0 = (mode == ONENET_LED_ON || mode == ONENET_LED_BLINK) ? 0 : 1;
    }

    if (mode == ONENET_LED_BLINK &&
        (now - last_toggle) >= pdMS_TO_TICKS(500u))
    {
        last_toggle = now;
        LED0 = !LED0;
    }
}

static u8 CloudConnect(void)
{
    printf("ESP8266 init...\r\n");
    if (ESP8266_Init() != ESP8266_OK)
    {
        printf("ESP8266 AT handshake failed\r\n");
        return ESP8266_ERR;
    }

    if (ESP8266_ConnectWifi((u8 *)ESP8266_WIFI_SSID,
                            (u8 *)ESP8266_WIFI_PASSWORD) != ESP8266_OK)
    {
        printf("ESP8266 WiFi connect failed\r\n");
        return ESP8266_ERR;
    }

    printf("ESP8266 WiFi OK, connect OneNet...\r\n");
    return OneNet_Init();
}

static void SensorTask(void *argument)
{
    SensorSnapshot snapshot;
    TickType_t last_wake;
    u8 dht_period = 0;

    (void)argument;
    snapshot.temperature = 0;
    snapshot.humidity = 0;
    snapshot.dht11_valid = 0;

    MQ2_CalibrateR0();
    last_wake = xTaskGetTickCount();

    for (;;)
    {
        snapshot.gas_raw = MQ2_Get_Raw_Value();
        snapshot.gas_ppm = MQ2_GetPPM_LPG(snapshot.gas_raw);
        snapshot.gas_leak = MQ2_Is_Gas_Leak_From_Raw(snapshot.gas_raw);

        snapshot.light_raw = LIGHT_Get_Raw_Value();
        snapshot.light_brightness = LIGHT_Brightness_From_Raw(snapshot.light_raw);
        snapshot.light_dark = LIGHT_Is_Dark_From_Raw(snapshot.light_raw);
        LIGHT_LED_Set(snapshot.light_dark);

        if (dht_period == 0u)
        {
            snapshot.dht11_valid =
                (DHT11_Read_Data(&snapshot.temperature, &snapshot.humidity) == 0u) ? 1u : 0u;
        }
        dht_period++;
        if (dht_period >= DHT11_SAMPLE_PERIODS)
        {
            dht_period = 0u;
        }

        snapshot.temp_alarm = 0u;
        snapshot.humidity_alarm = 0u;
        snapshot.alarm_mask = 0u;

        if (snapshot.gas_leak)
        {
            snapshot.alarm_mask |= ONENET_ALARM_GAS;
        }

        if (snapshot.dht11_valid)
        {
            if (snapshot.temperature > DHT11_TEMP_ALARM_HIGH)
            {
                snapshot.temp_alarm = 1u;
                snapshot.alarm_mask |= ONENET_ALARM_TEMP_HIGH;
            }
            else if (snapshot.temperature < DHT11_TEMP_ALARM_LOW)
            {
                snapshot.temp_alarm = 1u;
                snapshot.alarm_mask |= ONENET_ALARM_TEMP_LOW;
            }

            if (snapshot.humidity > DHT11_HUMI_ALARM_HIGH)
            {
                snapshot.humidity_alarm = 1u;
                snapshot.alarm_mask |= ONENET_ALARM_HUMIDITY_HIGH;
            }
            else if (snapshot.humidity < DHT11_HUMI_ALARM_LOW)
            {
                snapshot.humidity_alarm = 1u;
                snapshot.alarm_mask |= ONENET_ALARM_HUMIDITY_LOW;
            }
        }

        if (snapshot.alarm_mask != 0u)
        {
            BUZZER_ON();
        }
        else
        {
            BUZZER_OFF();
        }

        xQueueOverwrite(sensor_to_cloud_queue, &snapshot);
        xQueueOverwrite(sensor_to_display_queue, &snapshot);
        app_sensor_sample_count++;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}

static void CloudTask(void *argument)
{
    SensorSnapshot latest;
    TickType_t last_upload;
    TickType_t now;
    u8 have_sample = 0u;
    u8 connected = 0u;
    u8 reported_alarm_mask = 0u;
    u8 new_alarm_mask;

    (void)argument;
    last_upload = xTaskGetTickCount();

    for (;;)
    {
        if (!connected)
        {
            connected = (CloudConnect() == ESP8266_OK) ? 1u : 0u;
            last_upload = xTaskGetTickCount();
            if (!connected)
            {
                vTaskDelay(pdMS_TO_TICKS(CLOUD_RETRY_PERIOD_MS));
                continue;
            }
            app_cloud_connect_count++;
        }

        if (xQueueReceive(sensor_to_cloud_queue, &latest,
                          pdMS_TO_TICKS(50u)) == pdPASS)
        {
            app_cloud_sample_count++;
            if (!have_sample)
            {
                printf("Cloud sample ready: T=%u H=%u gas=%.1f light=%u\r\n",
                       latest.temperature, latest.humidity,
                       latest.gas_ppm, latest.light_brightness);
            }
            have_sample = 1u;
        }

        OneNet_Process();
        LED0_Update();

        if (!have_sample)
        {
            continue;
        }

        reported_alarm_mask &= latest.alarm_mask;
        new_alarm_mask = latest.alarm_mask & (u8)(~reported_alarm_mask);
        if (new_alarm_mask != 0u)
        {
            if (OneNet_SendAlarmEvent(new_alarm_mask,
                                      (float)latest.temperature,
                                      (float)latest.humidity,
                                      latest.gas_ppm) == ESP8266_OK)
            {
                reported_alarm_mask |= new_alarm_mask;
            }
            else
            {
                connected = 0u;
                continue;
            }
        }

        now = xTaskGetTickCount();
        if ((now - last_upload) >= pdMS_TO_TICKS(CLOUD_UPLOAD_PERIOD_MS))
        {
            last_upload = now;
            if (OneNet_SendData((float)latest.temperature,
                                (float)latest.humidity,
                                latest.gas_ppm,
                                (float)latest.light_brightness,
                                latest.alarm_mask) != ESP8266_OK)
            {
                app_cloud_upload_fail_count++;
                printf("OneNet offline, reconnecting...\r\n");
                connected = 0u;
            }
            else
            {
                app_cloud_upload_ok_count++;
            }
        }
    }
}

static void DisplayTask(void *argument)
{
    SensorSnapshot snapshot;
    char temp_text[24];
    char humidity_text[24];
    char light_text[24];

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(sensor_to_display_queue, &snapshot,
                          portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        LCD_ShowxNum(126, 130, snapshot.gas_raw, 4, 16, 0);
        LCD_ShowxNum(94, 150, (u16)snapshot.gas_ppm, 5, 16, 0);
        LCD_ShowString(118, 170, 200, 16, 16,
                       (u8 *)(snapshot.gas_leak ? "LEAK!" : "OK   "));

        sprintf(light_text, "LIGHT:%3d%% %s", snapshot.light_brightness,
                snapshot.light_dark ? "ON" : "OFF");
        LCD_ShowString(30, 270, 200, 16, 16, (u8 *)light_text);

        if (snapshot.dht11_valid)
        {
            sprintf(temp_text, "DHT11_TEMP:%dC", snapshot.temperature);
            sprintf(humidity_text, "DHT11_HUMI:%d%%", snapshot.humidity);
            LCD_ShowString(30, 190, 200, 16, 16, (u8 *)temp_text);
            LCD_ShowString(30, 210, 200, 16, 16, (u8 *)humidity_text);

            POINT_COLOR = snapshot.temp_alarm ? RED : GREEN;
            LCD_ShowString(30, 230, 200, 16, 16,
                           (u8 *)(snapshot.temp_alarm ? "TEMP_ALM:ALM" : "TEMP_ALM:OK "));
            POINT_COLOR = snapshot.humidity_alarm ? RED : GREEN;
            LCD_ShowString(30, 250, 200, 16, 16,
                           (u8 *)(snapshot.humidity_alarm ? "HUMI_ALM:ALM" : "HUMI_ALM:OK "));
        }
        else
        {
            POINT_COLOR = RED;
            LCD_ShowString(30, 190, 200, 16, 16, "DHT11_TEMP:ERR");
            LCD_ShowString(30, 210, 200, 16, 16, "DHT11_HUMI:ERR");
            LCD_ShowString(30, 230, 200, 16, 16, "TEMP_ALM:ERR");
            LCD_ShowString(30, 250, 200, 16, 16, "HUMI_ALM:ERR");
        }
        POINT_COLOR = BLUE;
    }
}

u8 AppTasks_Create(void)
{
    BaseType_t result;

    sensor_to_cloud_queue = xQueueCreate(1u, sizeof(SensorSnapshot));
    sensor_to_display_queue = xQueueCreate(1u, sizeof(SensorSnapshot));
    if (sensor_to_cloud_queue == NULL || sensor_to_display_queue == NULL)
    {
        return 1u;
    }

    result = xTaskCreate(SensorTask, "sensor", SENSOR_TASK_STACK_WORDS,
                         NULL, SENSOR_TASK_PRIORITY, NULL);
    if (result != pdPASS)
    {
        return 1u;
    }

    result = xTaskCreate(CloudTask, "cloud", CLOUD_TASK_STACK_WORDS,
                         NULL, CLOUD_TASK_PRIORITY, NULL);
    if (result != pdPASS)
    {
        return 1u;
    }

    result = xTaskCreate(DisplayTask, "display", DISPLAY_TASK_STACK_WORDS,
                         NULL, DISPLAY_TASK_PRIORITY, NULL);
    return (result == pdPASS) ? 0u : 1u;
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
