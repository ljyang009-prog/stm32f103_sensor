# STM32 Sensor Qt Host

这是一个配合 `sensor_collect` 固件使用的 Qt Widgets 串口上位机。下位机每 250ms 通过 USART1 发送三行文本，本程序解析后在界面中显示实时数值、报警状态和历史曲线。

## 界面功能

- 串口选择、刷新端口、波特率选择，默认 `115200 / 8N1`
- 实时显示 MQ-2 气体电压与泄漏状态
- MQ-2 可选 LPG / CO / 烟雾，并在干净空气中标定后显示估算 ppm
- 实时显示 DHT11 温度、湿度及上下限报警
- 实时显示光敏电阻亮度百分比、ADC 原始值和明暗状态
- 最近 120 秒历史曲线，可切换气体电压、温度、湿度、亮度
- 原始串口日志、时间戳开关、清空数据、导出 CSV
- 有泄漏或温湿度越限时顶部显示红色报警条

## 下位机串口输出格式

程序解析当前 `main.c` 中已有的输出：

```text
Gas leakage!!! ppm=120.5
Gas not leakage!!! ppm=35.2
DHT11: temp=26C humi=64% temp_alarm=0 humi_alarm=0
LIGHT: raw=830 level=72% dark=0
```

气体支持当前固件发送的 `ppm` 格式，也兼容旧版 `ad_value:xxV` 格式。

## 编译

要求：Qt 5.15 或 Qt 6，安装 Qt Serial Port 模块。

### Qt Creator

1. 打开 `qt_host/sensor_host.pro`
2. 选择套件后构建并运行

### CMake

```bash
cd qt_host
cmake -S . -B build
cmake --build build
./build/sensor_host
```

### qmake

```bash
cd qt_host
qmake
make
./sensor_host
```

## 虚拟机串口说明

如果 STM32 通过 USB 转串口连接到虚拟机，需要先在虚拟机设置里把 USB 串口设备映射进虚拟机，然后点击上位机的 Refresh 刷新端口列表。Linux 下常见端口名为 `/dev/ttyUSB0` 或 `/dev/ttyACM0`。

如果上位机在 Windows 宿主上运行，端口名通常是 `COM3`、`COM4` 等。

## MQ-2 浓度换算说明

MQ-2 的模拟电压是传感器分压输出，本身不是浓度。上位机按 MQ-2 数据手册曲线估算 `ppm`，但需要先选择目标气体，并在没有目标气体的干净空气中点击 `Calibrate clean air`。

当前换算按模块供电 `3.3V`、负载电阻 `10kOhm`、干净空气 `Rs/R0 = 9.83` 估算。不同模块和供电电压会导致数值有偏差，严格测量应使用标准气体标定。
