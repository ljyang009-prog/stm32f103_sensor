# stm32f103_sensor

STM32F103 多传感器采集、Qt 上位机监控与 OneNET 上云的完整项目总仓库。

## 项目一句话

基于 **STM32F103** 搭建多传感器监测系统，完成 **DHT11、MQ-2、光敏电阻** 采集、LCD 实时显示、异常报警，并通过 **ESP8266 + MQTT** 上报至 **OneNET**；同步开发 **Qt 串口上位机** 实现数据解析、历史曲线与日志导出。

## 仓库结构

- `sensor_collect/`：STM32 采集固件，负责传感器读取、LCD 显示、报警与串口上报
- `qt_host/`：Qt 串口上位机，负责串口接收、状态展示、历史曲线和 CSV 导出

## 主要功能

- **DHT11** 温湿度采集与越限报警
- **MQ-2** 气体检测与浓度估算
- **光敏电阻** 光照检测与补光控制
- **LCD** 实时数据显示
- **蜂鸣器 / 指示灯** 联动告警
- **ESP8266 + MQTT** 云端数据上报
- **Qt 上位机** 串口监控与历史曲线展示

## 技术栈

**STM32F103、HAL 库、C 语言、USART、ADC、GPIO、Timer、ESP8266、MQTT、OneNET、Qt、Qt Serial Port、Keil MDK**

## 使用顺序

1. 先看 `sensor_collect/README.md`
2. 再看 `qt_host/README.md`
3. 最后按串口协议和接线图联调

## 子模块说明

这个仓库是一个**展示型聚合仓库**，两个工程文件夹都以普通目录形式保留在仓库内，便于直接浏览、编译和演示。

## 链接

- 总仓库：`https://github.com/ljyang009-prog/stm32f103_sensor`
- 采集固件仓库：`https://github.com/ljyang009-prog/sensor_collect`

