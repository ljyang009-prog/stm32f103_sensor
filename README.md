# stm32f103_sensor

STM32F103 多传感器采集、上位机监控与云平台上报的完整项目总仓库。

## 项目概述

本项目由 **STM32 传感器采集端**、**Qt 串口上位机** 和 **云平台通信链路** 组成，完成温湿度、气体浓度与光照数据的采集、显示、报警和远程监测。

## 仓库组成

- `sensor_collect/`：STM32F103 采集固件，负责传感器读取、LCD 显示、报警、串口上报
- `qt_host/`：Qt 串口上位机，负责数据解析、实时展示、历史曲线和日志导出

## 功能亮点

- **DHT11** 温湿度采集与越限报警
- **MQ-2** 气体检测与浓度换算
- **光敏电阻** 光照检测与补光控制
- **LCD** 实时数据显示
- **ESP8266 + MQTT** 云端上报
- **Qt 上位机** 串口监控与历史曲线展示

## 技术栈

**STM32F103、HAL 库、C 语言、USART、ADC、GPIO、Timer、ESP8266、MQTT、OneNET、Qt、Qt Serial Port、Keil MDK**

## 推荐阅读顺序

1. 先看 `sensor_collect/README.md`
2. 再看 `qt_host/README.md`
3. 最后根据接线图和串口协议进行联调

## 子仓库链接

- 采集固件仓库：`https://github.com/ljyang009-prog/sensor_collect`
- 对外展示总仓库：`https://github.com/ljyang009-prog/stm32f103_sensor`

## 简历表述建议

如果用于简历，可写成：

> 基于 **STM32F103** 搭建多传感器采集与监测系统，完成 **DHT11、MQ-2、光敏电阻** 数据采集、异常报警、LCD 实时显示，并通过 **ESP8266 + MQTT** 上报至 **OneNET**；同步开发 **Qt 串口上位机** 实现数据解析、历史曲线展示与日志导出。

