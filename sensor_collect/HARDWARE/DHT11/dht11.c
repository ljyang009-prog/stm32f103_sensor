#include "dht11.h"
#include "delay.h"

static void DHT11_IO_IN(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    GPIO_Initure.Pin = DHT11_DQ_GPIO_PIN;
    GPIO_Initure.Mode = GPIO_MODE_INPUT;
    GPIO_Initure.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_DQ_GPIO_PORT, &GPIO_Initure);
}

static void DHT11_IO_OUT(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    GPIO_Initure.Pin = DHT11_DQ_GPIO_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DQ_GPIO_PORT, &GPIO_Initure);
}

void DHT11_Rst(void)
{
    DHT11_IO_OUT();
    DHT11_DQ_OUT(0);
    delay_ms(20);
    DHT11_DQ_OUT(1);
    delay_us(30);
}

u8 DHT11_Check(void)
{
    u8 retry = 0;

    DHT11_IO_IN();
    while (DHT11_DQ_IN() == GPIO_PIN_SET && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100)
    {
        return 1;
    }

    retry = 0;
    while (DHT11_DQ_IN() == GPIO_PIN_RESET && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100)
    {
        return 1;
    }

    return 0;
}

u8 DHT11_Read_Bit(void)
{
    u8 retry = 0;

    while (DHT11_DQ_IN() == GPIO_PIN_SET && retry < 100)
    {
        retry++;
        delay_us(1);
    }

    retry = 0;
    while (DHT11_DQ_IN() == GPIO_PIN_RESET && retry < 100)
    {
        retry++;
        delay_us(1);
    }

    delay_us(40);
    return (DHT11_DQ_IN() == GPIO_PIN_SET) ? 1 : 0;
}

u8 DHT11_Read_Byte(void)
{
    u8 i;
    u8 dat = 0;

    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        dat |= DHT11_Read_Bit();
    }
    return dat;
}

u8 DHT11_Read_Data(u8 *temp, u8 *humi)
{
    u8 buf[5];
    u8 i;

    DHT11_Rst();
    if (DHT11_Check() == 0)
    {
        for (i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();
        }

        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *humi = buf[0];
            *temp = buf[2];
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

u8 DHT11_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    DHT11_Rst();
    return DHT11_Check();
}
