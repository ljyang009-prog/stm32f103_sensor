/**
 * OneNET Studio 物模型 driver (MQTT protocol).
 *
 * Flow:
 *   1. ESP8266 opens a TCP client to the OneNET Studio MQTT server.
 *   2. MQTT CONNECT (username = product id, password = device key,
 *      client id = device name).
 *   3. Periodically publish properties to topic
 *      "$sys/{pid}/{device_name}/thing/property/post".
 */

#include "onenet.h"
#include "mqttkit.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

static u32 onenet_msg_id = 0;

/* Open the TCP connection and finish the MQTT login handshake. */
u8 OneNet_Init(void)
{
    MQTT_PACKET_STRUCTURE mqttPacket;
    u8 connack[8];
    u16 n;
    u32 t0;

    mqttPacket._data = NULL;
    mqttPacket._len = 0;
    mqttPacket._size = 0;
    mqttPacket._memFlag = MEM_FLAG_NULL;

    /* 1. TCP connect to the OneNET Studio MQTT server. */
    if (ESP8266_ConnectTCP((u8 *)ONENET_SERVER_IP, ONENET_SERVER_PORT) != ESP8266_OK)
    {
        printf("OneNet TCP connect failed\r\n");
        return ESP8266_ERR;
    }

    /* 2. Build the MQTT CONNECT packet.
     *    user = product id, password = token, devid(clientId) = device name. */
    if (MQTT_PacketConnect((int8 *)ONENET_PROID, (int8 *)ONENET_TOKEN,
                           (int8 *)ONENET_DEVID, 256, 0, MQTT_QOS_LEVEL0,
                           NULL, NULL, 0, &mqttPacket) != 0)
    {
        printf("MQTT connect packet failed\r\n");
        return ESP8266_ERR;
    }

    /* 3. Send it. */
    ESP8266_SendTcpData(0, mqttPacket._data, (u16)mqttPacket._len);
    MQTT_DeleteBuffer(&mqttPacket);

    /* 4. Wait for CONNACK. */
    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) <= 5000)
    {
        n = ESP8266_GetTcpData(connack, sizeof(connack));
        if (n >= 4 && connack[0] == 0x20 && connack[1] == 0x02)
        {
            if (connack[3] == 0x00)
            {
                printf("OneNet MQTT connected\r\n");
                return ESP8266_OK;
            }
            printf("OneNet MQTT refused, code=%d\r\n", connack[3]);
            return ESP8266_ERR;
        }
        delay_ms(10);
    }

    printf("OneNet CONNACK timeout\r\n");
    return ESP8266_ERR;
}

/* 物模型属性上报：把 4 个传感器值打包成 OneJSON 发到 property/post topic。 */
u8 OneNet_SendData(float temperature, float humidity, float gas, float light)
{
    MQTT_PACKET_STRUCTURE mqttPacket;
    char topic[96];
    char payload[256];

    mqttPacket._data = NULL;
    mqttPacket._len = 0;
    mqttPacket._size = 0;
    mqttPacket._memFlag = MEM_FLAG_NULL;

    onenet_msg_id++;

    sprintf(payload,
        "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
        "\"temperature\":{\"value\":%.1f},"
        "\"humidity\":{\"value\":%.1f},"
        "\"gas\":{\"value\":%.1f},"
        "\"light\":{\"value\":%.0f}}}",
        onenet_msg_id, temperature, humidity, gas, light);

    sprintf(topic, "$sys/%s/%s/thing/property/post", ONENET_PROID, ONENET_DEVID);

    if (MQTT_PacketPublish(MQTT_PUBLISH_ID, (int8 *)topic, (int8 *)payload,
                           (uint32)strlen(payload), MQTT_QOS_LEVEL0, 0, 0,
                           &mqttPacket) != 0)
    {
        return ESP8266_ERR;
    }

    ESP8266_SendTcpData(0, mqttPacket._data, (u16)mqttPacket._len);
    MQTT_DeleteBuffer(&mqttPacket);

    return ESP8266_OK;
}

/* Retarget: semihosting stub required by the ARM C library when
 * __use_no_semihosting is enabled and malloc() is linked in (mqttkit.c). */
void _ttywrch(int ch)
{
    ch = ch;
}
