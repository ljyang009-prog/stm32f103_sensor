/**
 * OneNET Studio thing-model driver (MQTT over an ESP-AT SSL socket).
 *
 * Flow:
 *   1. Open a TLS socket with AT+CIPSTART="SSL".
 *   2. Build and send MQTT packets on the STM32 (supports long OneNET tokens).
 *   3. Publish the 4 sensor values as OneJSON to
 *      "$sys/{pid}/{device}/thing/property/post".
 *
 * client_id = device name, username = product id, password = token.
 */

#include "onenet.h"
#include "mqttkit.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

static u32 onenet_msg_id = 0;
static volatile u8 onenet_led_mode = ONENET_LED_OFF;

#define ONENET_PROPERTY_SET_TOPIC \
    "$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/property/set"
#define ONENET_PROPERTY_SET_REPLY_TOPIC \
    "$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/property/set_reply"
#define ONENET_PROPERTY_POST_REPLY_TOPIC \
    "$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/property/post/reply"
#define ONENET_EVENT_POST_TOPIC \
    "$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/event/post"
#define ONENET_EVENT_POST_REPLY_TOPIC \
    "$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/event/post/reply"

static void OneNet_ResetPacket(MQTT_PACKET_STRUCTURE *mqttPacket)
{
    mqttPacket->_data = NULL;
    mqttPacket->_len = 0;
    mqttPacket->_size = 0;
    mqttPacket->_memFlag = MEM_FLAG_NULL;
}

static u8 OneNet_Publish(const char *topic, const char *payload)
{
    MQTT_PACKET_STRUCTURE mqttPacket;
    u8 result;
    u8 packet_result;
    u32 packet_size;

    OneNet_ResetPacket(&mqttPacket);
    packet_size = (u32)strlen(topic) + (u32)strlen(payload) + 5u;
    packet_result = MQTT_PacketPublish(MQTT_PUBLISH_ID, (int8 *)topic,
                                       (int8 *)payload,
                                       (uint32)strlen(payload),
                                       MQTT_QOS_LEVEL0, 0, 0, &mqttPacket);
    if (packet_result != 0)
    {
        printf("MQTT publish build failed, code=%u need=%u free=%u\r\n",
               packet_result, packet_size, (u32)xPortGetFreeHeapSize());
        return ESP8266_ERR;
    }

    result = ESP8266_SendTcpData(ONENET_MQTT_LINK, mqttPacket._data,
                                 (u16)mqttPacket._len);
    MQTT_DeleteBuffer(&mqttPacket);
    return result;
}

static u8 OneNet_SubscribePropertySet(void)
{
    MQTT_PACKET_STRUCTURE mqttPacket;
    const int8 *topics[3];
    u8 suback[16];
    u16 n;
    u32 t0;
    u8 i;

    OneNet_ResetPacket(&mqttPacket);
    topics[0] = (const int8 *)ONENET_PROPERTY_SET_TOPIC;
    topics[1] = (const int8 *)ONENET_PROPERTY_POST_REPLY_TOPIC;
    topics[2] = (const int8 *)ONENET_EVENT_POST_REPLY_TOPIC;
    if (MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0,
                             topics, 3, &mqttPacket) != 0)
    {
        printf("OneNet property subscription build failed\r\n");
        return ESP8266_ERR;
    }

    if (ESP8266_SendTcpData(ONENET_MQTT_LINK, mqttPacket._data,
                            (u16)mqttPacket._len) != ESP8266_OK)
    {
        MQTT_DeleteBuffer(&mqttPacket);
        printf("OneNet property subscription send failed\r\n");
        return ESP8266_ERR;
    }
    MQTT_DeleteBuffer(&mqttPacket);

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) <= 5000)
    {
        n = ESP8266_GetTcpData(suback, sizeof(suback));
        if (n >= 7 && suback[0] == 0x90 && suback[1] == 0x05 &&
            suback[2] == MOSQ_MSB(MQTT_SUBSCRIBE_ID) &&
            suback[3] == MOSQ_LSB(MQTT_SUBSCRIBE_ID))
        {
            for (i = 0; i < 3; i++)
            {
                if (suback[4 + i] == 0x80)
                {
                    printf("OneNet subscription %u refused\r\n", i);
                    return ESP8266_ERR;
                }
            }
            printf("OneNet control and reply topics subscribed\r\n");
            return ESP8266_OK;
        }
        delay_ms(10);
    }

    printf("OneNet topic subscription timeout\r\n");
    return ESP8266_ERR;
}

static void OneNet_DumpEspRx(void)
{
    u8 raw[64];
    u16 got;
    u16 i;
    u16 total = 0;

    while ((got = ESP8266_ReadRx(raw, sizeof(raw))) != 0)
    {
        if (total == 0)
            printf("ESP RX HEX:");
        for (i = 0; i < got; i++)
            printf(" %02X", raw[i]);
        total = (u16)(total + got);
    }

    if (total == 0)
        printf("ESP RX HEX: <empty>");
    printf("\r\n");
}

/* Open the TLS connection and finish the MQTT login handshake. */
u8 OneNet_Init(void)
{
    MQTT_PACKET_STRUCTURE mqttPacket;
    u8 connack[8];
    u16 n;
    u32 t0;

    OneNet_ResetPacket(&mqttPacket);

    /* CloudConnect resets the ESP8266 before each connection attempt, so no
     * stale socket can remain here. Avoid a two-second close timeout. */

    printf("OneNet TLS connect %s:%u...\r\n",
           ONENET_SERVER_HOST, ONENET_SERVER_PORT);
    if (ESP8266_ConnectSSL((u8 *)ONENET_SERVER_HOST,
                           ONENET_SERVER_PORT) != ESP8266_OK)
    {
        printf("OneNet TLS socket failed; firmware must support AT+CIPSTART SSL\r\n");
        return ESP8266_ERR;
    }

    /* OneNET Studio requires a clean MQTT 3.1.1 session for device login. */
    if (MQTT_PacketConnect((int8 *)ONENET_PROID, (int8 *)ONENET_TOKEN,
                           (int8 *)ONENET_DEVID, 256, 1, MQTT_QOS_LEVEL0,
                           NULL, NULL, 0, &mqttPacket) != 0)
    {
        printf("OneNet MQTT packet build failed\r\n");
        return ESP8266_ERR;
    }

    if (ESP8266_SendTcpData(ONENET_MQTT_LINK, mqttPacket._data,
                            (u16)mqttPacket._len) != ESP8266_OK)
    {
        MQTT_DeleteBuffer(&mqttPacket);
        printf("OneNet MQTT CONNECT send failed\r\n");
        return ESP8266_ERR;
    }
    MQTT_DeleteBuffer(&mqttPacket);

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) <= 10000)
    {
        n = ESP8266_GetTcpData(connack, sizeof(connack));
        if (n >= 4 && connack[0] == 0x20 && connack[1] == 0x02)
        {
            if (connack[3] == 0x00)
            {
                printf("OneNet MQTT connected\r\n");
                return OneNet_SubscribePropertySet();
            }
            printf("OneNet MQTT refused, CONNACK=%u\r\n", connack[3]);
            return ESP8266_ERR;
        }
        delay_ms(10);
    }

    printf("OneNet MQTT CONNACK timeout\r\n");
    OneNet_DumpEspRx();

    if (ESP8266_SendCmd((u8 *)"AT+CIPSTATUS", (u8 *)"OK", 3000) == ESP8266_OK)
    {
        printf("ESP CIPSTATUS ");
        OneNet_DumpEspRx();
    }
    return ESP8266_ERR;
}

static u8 OneNet_TemperatureAlarm(u8 alarm_mask)
{
    if (alarm_mask & ONENET_ALARM_TEMP_HIGH)
        return 2;
    if (alarm_mask & ONENET_ALARM_TEMP_LOW)
        return 1;
    return 0;
}

static u8 OneNet_HumidityAlarm(u8 alarm_mask)
{
    if (alarm_mask & ONENET_ALARM_HUMIDITY_HIGH)
        return 2;
    if (alarm_mask & ONENET_ALARM_HUMIDITY_LOW)
        return 1;
    return 0;
}

/* Keep the periodic report limited to identifiers present in the thing model. */
u8 OneNet_SendData(float temperature, float humidity, float gas, float light,
                   u8 alarm_mask)
{
    char topic[96];
    static char payload[256];

    onenet_msg_id++;

    sprintf(payload,
        "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
        "\"temperature\":{\"value\":%.1f},"
        "\"humidity\":{\"value\":%.1f},"
        "\"gas\":{\"value\":%.1f},"
        "\"light\":{\"value\":%.0f},"
        "\"led_mode\":{\"value\":%u}}}",
        onenet_msg_id, temperature, humidity, gas, light, onenet_led_mode);

    sprintf(topic, "$sys/%s/%s/thing/property/post", ONENET_PROID, ONENET_DEVID);
    if (OneNet_Publish(topic, payload) != ESP8266_OK)
    {
        printf("OneNet publish failed\r\n");
        return ESP8266_ERR;
    }

    printf("OneNet property packet sent, id=%u alarm=0x%02X heap=%u\r\n",
           onenet_msg_id, alarm_mask, (u32)xPortGetFreeHeapSize());
    return ESP8266_OK;
}

/* Send one edge-triggered event containing every source that just triggered. */
u8 OneNet_SendAlarmEvent(u8 trigger_mask, float temperature,
                         float humidity, float gas)
{
    static char payload[384];

    if (trigger_mask == 0)
        return ESP8266_OK;

    onenet_msg_id++;
    sprintf(payload,
        "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
        "\"alarm_event\":{\"value\":{"
        "\"trigger_mask\":%u,"
        "\"gas_alarm\":%u,"
        "\"temperature_alarm\":%u,"
        "\"humidity_alarm\":%u,"
        "\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"gas\":%.1f}}}}",
        onenet_msg_id, trigger_mask,
        (trigger_mask & ONENET_ALARM_GAS) ? 1 : 0,
        OneNet_TemperatureAlarm(trigger_mask),
        OneNet_HumidityAlarm(trigger_mask),
        temperature, humidity, gas);

    if (OneNet_Publish(ONENET_EVENT_POST_TOPIC, payload) != ESP8266_OK)
    {
        printf("OneNet alarm event failed, mask=0x%02X\r\n", trigger_mask);
        return ESP8266_ERR;
    }

    printf("OneNet alarm event sent, mask=0x%02X\r\n", trigger_mask);
    return ESP8266_OK;
}

static u8 OneNet_DecodePublish(const u8 *packet, u16 packet_len,
                               char *topic, u16 topic_size,
                               char *payload, u16 payload_size)
{
    u32 remain_len = 0;
    u32 multiplier = 1;
    u16 index = 1;
    u16 packet_end;
    u16 topic_len;
    u16 payload_len;
    u8 encoded;
    u8 qos;

    if (packet_len < 4 || (packet[0] >> 4) != MQTT_PKT_PUBLISH)
        return 0;

    do
    {
        if (index >= packet_len || multiplier > 2097152)
            return 0;
        encoded = packet[index++];
        remain_len += (u32)(encoded & 0x7F) * multiplier;
        multiplier *= 128;
    } while ((encoded & 0x80) != 0);

    if (remain_len > (u32)(packet_len - index) || remain_len < 2)
        return 0;
    packet_end = (u16)(index + remain_len);

    topic_len = (u16)(((u16)packet[index] << 8) | packet[index + 1]);
    index = (u16)(index + 2);
    if (topic_len == 0 || topic_len >= topic_size ||
        topic_len > (u16)(packet_end - index))
        return 0;

    memcpy(topic, packet + index, topic_len);
    topic[topic_len] = '\0';
    index = (u16)(index + topic_len);

    qos = (u8)((packet[0] >> 1) & 0x03);
    if (qos != MQTT_QOS_LEVEL0)
    {
        if ((u16)(index + 2) > packet_end)
            return 0;
        index = (u16)(index + 2);
    }

    payload_len = (u16)(packet_end - index);
    if (payload_len >= payload_size)
        return 0;
    memcpy(payload, packet + index, payload_len);
    payload[payload_len] = '\0';
    return 1;
}

static void OneNet_ExtractId(const char *payload, char *id, u16 id_size)
{
    const char *p;
    u16 n = 0;

    id[0] = '0';
    id[1] = '\0';
    p = strstr(payload, "\"id\"");
    if (p == NULL)
        return;
    p = strchr(p + 4, ':');
    if (p == NULL)
        return;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\"')
        p++;
    while (*p != '\0' && *p != '\"' && *p != ',' && *p != '}' &&
           n < (u16)(id_size - 1))
        id[n++] = *p++;
    id[n] = '\0';
}

static u8 OneNet_ReplyAccepted(const char *payload)
{
    const char *p;

    p = strstr(payload, "\"code\"");
    if (p == NULL)
        return 0;
    p = strchr(p + 6, ':');
    if (p == NULL)
        return 0;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    return (p[0] == '2' && p[1] == '0' && p[2] == '0' &&
            (p[3] == ',' || p[3] == '}' || p[3] == ' ' ||
             p[3] == '\t' || p[3] == '\r' || p[3] == '\n'));
}

static u8 OneNet_ParseLedMode(const char *payload, u8 *mode)
{
    const char *p;
    const char *value;
    int number = 0;

    p = strstr(payload, "\"led_mode\"");
    if (p == NULL)
        return 0;
    p = strchr(p + 10, ':');
    if (p == NULL)
        return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;

    if (*p == '{')
    {
        value = strstr(p, "\"value\"");
        if (value == NULL)
            return 0;
        p = strchr(value + 7, ':');
        if (p == NULL)
            return 0;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
    }

    if (*p < '0' || *p > '9')
        return 0;
    while (*p >= '0' && *p <= '9')
    {
        number = number * 10 + (*p - '0');
        p++;
    }
    if (number < ONENET_LED_OFF || number > ONENET_LED_BLINK)
        return 0;

    *mode = (u8)number;
    return 1;
}

u8 OneNet_Process(void)
{
    static u8 packet[512];
    static char topic[96];
    static char payload[320];
    static char reply[128];
    static char id[32];
    u16 packet_len;
    u8 mode;
    u8 valid;

    packet_len = ESP8266_GetTcpData(packet, sizeof(packet));
    if (packet_len == 0)
        return ESP8266_OK;
    if (!OneNet_DecodePublish(packet, packet_len, topic, sizeof(topic),
                              payload, sizeof(payload)))
        return ESP8266_OK;
    if (strcmp(topic, ONENET_EVENT_POST_REPLY_TOPIC) == 0)
    {
        printf("OneNet alarm event reply: %s\r\n", payload);
        return ESP8266_OK;
    }
    if (strcmp(topic, ONENET_PROPERTY_POST_REPLY_TOPIC) == 0)
    {
        if (OneNet_ReplyAccepted(payload))
            printf("OneNet cloud confirmed property: %s\r\n", payload);
        else
            printf("OneNet cloud rejected property: %s\r\n", payload);
        return ESP8266_OK;
    }
    if (strcmp(topic, ONENET_PROPERTY_SET_TOPIC) != 0)
        return ESP8266_OK;

    OneNet_ExtractId(payload, id, sizeof(id));
    valid = OneNet_ParseLedMode(payload, &mode);
    if (valid)
    {
        onenet_led_mode = mode;
        sprintf(reply, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id);
        printf("OneNet LED mode=%u\r\n", mode);
    }
    else
    {
        sprintf(reply, "{\"id\":\"%s\",\"code\":400,\"msg\":\"invalid led_mode\"}", id);
        printf("OneNet invalid LED command: %s\r\n", payload);
    }

    return OneNet_Publish(ONENET_PROPERTY_SET_REPLY_TOPIC, reply);
}

u8 OneNet_GetLedMode(void)
{
    return onenet_led_mode;
}

/* mqttkit.c is still part of the Keil target and pulls in the ARM C library. */
void _ttywrch(int ch)
{
    ch = ch;
}
