# STM32F103 多传感器采集与 OneNET 云平台系统

基于 STM32F103（Mini STM32 开发板）的多传感器采集系统：采集温湿度、有害气体浓度、光照强度，在 TFT 液晶上实时显示，并通过 **ESP8266 + MQTT** 上报到 **OneNET Studio 云平台**（物模型）。

---

## 功能特性

- **DHT11** 温湿度采集，带上下限报警
- **MQ-2** 烟雾/可燃气体检测，ADC 电压换算成 **气体浓度(ppm)**
- **光敏传感器** 光照亮度检测，自动控制补光 LED
- **蜂鸣器** 温度/湿度/气体泄漏报警
- **TFT LCD** 实时显示各项数据与报警状态
- **ESP8266 + OneNET Studio** 通过 MQTT(TLS) 上报数据，云端可视化
- **FreeRTOS** 将传感器采集、LCD 显示和云通信解耦，网络重连不会阻塞采集
- **OneNET 下行控制** 支持远程设置 LED0 的关闭、常亮和闪烁模式

---

## 硬件组成

| 模块 | 说明 |
|------|------|
| 主控 | STM32F103RC（Mini STM32 开发板）|
| 温湿度 | DHT11 |
| 气体 | MQ-2（模拟量 + 数字量）|
| 光照 | 光敏电阻 + 补光 LED |
| 显示 | 2.4/2.8 寸 TFT LCD（ILI9341 等）|
| 通信 | ESP-01S（ESP-AT 固件，需支持 SSL socket）|
| 云平台 | OneNET Studio（物模型 / MQTT）|

---

## 引脚分配

### GPIOA
| 引脚 | 功能 | 说明 |
|------|------|------|
| PA0 | ESP8266 RST | 可选复位脚，低电平有效 |
| PA1 | MQ-2 模拟输出 | ADC1 通道 1 |
| PA2 | USART2_TX | → ESP8266 RX（发数据给 ESP8266）|
| PA3 | USART2_RX | ← ESP8266 TX |
| PA4 | MQ-2 数字输出 | DOUT，低电平表示泄漏 |
| PA5 | DHT11 数据线 | |
| PA7 | 光敏传感器 | ADC1 通道 7 |
| PA8 | LED0 | |
| PA9 / PA10 | USART1 TX/RX | 调试串口（printf）|

### 其它端口
| 引脚 | 功能 | 说明 |
|------|------|------|
| PB0~PB15 | LCD 数据总线 | 16 位并行 |
| PC1 | 蜂鸣器 | |
| PC3 | 光敏补光 LED | |
| PC6~PC10 | LCD 控制 | RD / WR / RS / CS / 背光 |
| PD2 | LED1 | |

> 完整原理图与 IO 分配见 `原理图及引脚分配/` 目录。

---

## 软件架构

当前 STM32 工程使用 ESP-AT 架构：

```
┌──────────┐  USART2(PA2)   ┌─────────────────────┐  WiFi(TLS)  ┌────────────┐
│  STM32   │ ──────────────► │  ESP8266            │ ───────────► │  OneNET    │
│ FreeRTOS │  AT指令 + MQTT报文 │  (ESP-AT 固件)       │   MQTT/TLS  │  Studio    │
│ 采集/显示 │  USART2(PA2/PA3)  │  WiFi + SSL socket   │              │  物模型     │
└──────────┘                 └─────────────────────┘             └────────────┘
```

- **STM32**（Keil 工程）：负责传感器采集，并通过 USART2 驱动 ESP-01S，生成 MQTT CONNECT/PUBLISH 报文。
- **ESP-01S**（ESP-AT 固件）：连接 WiFi，通过 `AT+CIPSTART="SSL"` 建立 OneNET TLS socket。

### FreeRTOS 任务

系统已将采集、显示和上云拆分为三个独立任务：

| 任务 | 优先级 | 功能 |
|------|--------|------|
| `sensor` | 4 | 每秒采集 MQ-2 和光照，每 2 秒采集 DHT11，并处理报警输出 |
| `display` | 3 | 从独立队列读取最新快照并刷新 LCD |
| `cloud` | 2 | 管理 ESP8266、OneNET MQTT、5 秒周期上报和断线重连 |

采集任务分别通过两个长度为 1 的覆盖队列向显示任务和云任务发布最新快照。网络等待只发生在低优先级云任务中，因此不会阻塞采集和 LCD 刷新。系统使用 `heap_4.c` 和 20 KiB FreeRTOS 堆，MQTT 报文也从 FreeRTOS 堆分配，避免 ARM C 库默认小堆导致大报文构建失败。

---

## 目录结构

```
sensor_collect/
├── CORE/               # CMSIS 内核 + 启动文件
├── HALLIB/             # STM32F1 HAL 库
├── HARDWARE/           # 外设驱动
│   ├── ADC/            # ADC
│   ├── BUZZER/         # 蜂鸣器
│   ├── DHT11/          # 温湿度
│   ├── ESP8266/        # ESP8266 串口驱动（USART2）
│   ├── LCD/            # TFT 液晶
│   ├── LED/            # LED
│   ├── LIGHT/          # 光敏传感器
│   ├── MQ2/            # 气体传感器（含 ppm 换算）
│   ├── MQTT/           # MQTT 协议组包库
│   └── ONENET/         # OneNET 接入（物模型）
├── FreeRTOS/            # FreeRTOS V9 内核、Cortex-M3 端口和 heap_4
├── SYSTEM/             # delay / sys / usart
├── USER/               # main、任务、配置示例和 Keil 工程
├── USMART/             # 串口调试组件
├── 原理图及引脚分配/     # 原理图与引脚分配表
└── README.md
```

---

## 编译与烧录

### 1. STM32（Keil MDK5）

1. 用 Keil MDK5 打开 `USER/ADC.uvprojx`。
2. 确认编译器为 ARM Compiler V5.06，器件为 `STM32F103RC`。
3. 编译工程，成功后生成 `OBJ/ADC.axf`、`OBJ/ADC.hex` 和 `OBJ/ADC.bin`。
4. 使用 CMSIS-DAP/ST-Link 下载。建议先将 SWD 时钟设为 1 MHz；连接不稳定时可降到 100 kHz。

固件启动后，USART1（PA9/PA10）以 115200 8-N-1 输出诊断日志。

### 2. ESP-01S（AT 固件）

1. 使用串口助手确认 `AT` 返回 `OK`。
2. 执行 `AT+CIPSTART=0,"SSL","<产品ID>.mqttstls.acc.cmcconenet.cn",8883`，必须返回 `CONNECT`/`OK`。
3. 若返回 `ERROR` 或不识别 `SSL`，需换用包含 SSL socket 支持的 ESP8266 AT 固件。
4. 把 ESP-01S 接回 STM32 USART2（PA2/PA3），注意 3.3V 供电峰值电流应至少 500mA。

---

## 配置说明

### 本地配置

复制配置示例，并只在本地文件中填写真实凭据：

```powershell
Copy-Item USER/app_config.example.h USER/app_config.h
```

编辑 `USER/app_config.h`：

```c
#define ESP8266_WIFI_SSID   "你的WiFi名称"
#define ESP8266_WIFI_PASSWORD "你的WiFi密码"
#define ONENET_PROID        "产品ID"
#define ONENET_DEVID        "设备名称"
#define ONENET_TOKEN        "生成的token"
#define ONENET_SERVER_HOST  "产品专属.mqttstls.acc.cmcconenet.cn"
```

`USER/app_config.h` 已加入 `.gitignore`，不会提交到仓库。不要把 Wi-Fi 密码、设备密钥或 token 写入公共源码；token 的 `et` 必须晚于设备使用时间。

### OneNET 平台配置

1. 登录 [OneNET Studio](https://open.iot.10086.cn) → 创建产品（MQTT 协议）。
2. 添加设备，得到 **产品ID**、**设备名称**、**设备密钥/access_key**（或生成 token）。
3. 配置物模型，添加以下 5 个基础属性，标识符和类型必须完全一致：

| 功能名称 | 标识符 | 数据类型 | 权限/单位 |
|---------|--------|---------|-----------|
| 温度 | `temperature` | double | 只读 / ℃ |
| 湿度 | `humidity` | double | 只读 / % |
| 有害气体浓度 | `gas` | double | 只读 / ppm |
| 光照亮度 | `light` | int32 | 只读 / % |
| LED 模式 | `led_mode` | int32/枚举 | 可读写；0 关闭、1 常亮、2 闪烁 |

4. 上报 topic：`$sys/{产品ID}/{设备名称}/thing/property/post`
5. 上报回复 topic：`$sys/{产品ID}/{设备名称}/thing/property/post/reply`
6. LED 属性设置 topic：`$sys/{产品ID}/{设备名称}/thing/property/set`
7. LED 属性设置回复 topic：`$sys/{产品ID}/{设备名称}/thing/property/set_reply`

如需将本地报警同步到云端，再创建事件 `alarm_event`，事件输出参数如下：

| 输出标识符 | 类型 | 含义 |
|-----------|------|------|
| `trigger_mask` | int32 | 本次新触发的报警位掩码 |
| `gas_alarm` | int32/布尔 | 燃气泄漏是否触发 |
| `temperature_alarm` | int32/枚举 | 0 无、1 温度过低、2 温度过高 |
| `humidity_alarm` | int32/枚举 | 0 无、1 湿度过低、2 湿度过高 |
| `temperature` | double | 触发时温度 |
| `humidity` | double | 触发时湿度 |
| `gas` | double | 触发时气体浓度 |

事件上报 topic 为 `$sys/{产品ID}/{设备名称}/thing/event/post`，回复 topic 为 `$sys/{产品ID}/{设备名称}/thing/event/post/reply`。报警位定义：`0x01` 燃气、`0x02` 温度过低、`0x04` 温度过高、`0x08` 湿度过低、`0x10` 湿度过高；多个报警可按位组合。

---

## 数据流（OneNET）

设备每 5 秒上报一次，包含传感器数据和当前 LED 模式：

| 数据流 | 含义 |
|--------|------|
| `temperature` | 温度 (℃) |
| `humidity` | 湿度 (%) |
| `gas` | 有害气体浓度 (ppm，由 MQ-2 电压换算) |
| `light` | 光照亮度 (%) |
| `led_mode` | LED0 模式：0 关闭、1 常亮、2 每 500 ms 闪烁 |

`OneNet property packet sent` 只表示 ESP8266 已发送 MQTT 报文；只有下面的 `code=200` 才表示平台真正接收了属性：

```text
OneNet cloud confirmed property: {"id":"1","code":200,"msg":"success"}
```

若出现 `code=2306` 和 `identifier not exist`，需要在 OneNET 物模型中创建对应标识符，或从上报报文中移除该字段。

---

## 本地报警

报警阈值定义在 `HARDWARE/DHT11/dht11.h`：温度低于 10 ℃或高于 35 ℃、湿度低于 20% 或高于 85% 时报警。比较使用严格的 `<`/`>`，刚好等于阈值不会触发。MQ-2 数字输出或模拟电压达到泄漏条件时也会报警。

任一报警置位后，采集任务持续驱动 PC1 有源低电平蜂鸣器；LCD 同时显示 `TEMP_ALM:ALM`、`HUMI_ALM:ALM` 或 `LEAK!`。蜂鸣器不响时先检查 PC1、VCC 和 GND 接线，再根据上报日志中的掩码确认软件是否触发：`0x02` 为低温，`0x04` 为高温。

---

## 常见问题

| 现象 | 检查项 |
|------|--------|
| `AT+CIPSTART SSL` 失败 | ESP-AT 固件是否支持 SSL、供电峰值是否达到 500 mA |
| `CIPMUX and CIPSERVER must be 0` | 固件会依次设置 `CIPMUX=0`、`CIPMODE=0`、`CIPMUX=1`；若仍失败请复位 ESP8266 |
| MQTT 已连接但云端无数据 | 以 `OneNet cloud confirmed property` 和 `code=200` 为准，检查物模型标识符与类型 |
| 下载时报 `RDDI-DAP Error`/`No ACK` | 降低 SWD 时钟，检查 NRST/SWDIO/SWCLK/GND，必要时按住 RESET 开始下载后松开 |
| 串口停在订阅日志 | 等待第一份传感器快照，并检查 USART2 接收和 FreeRTOS 堆余量 |

---

## 说明

- MQ-2 的 ppm 浓度是基于 `Rs/R0` 曲线拟合的**近似值**（未用标准气体标定），仅作趋势参考；上电时会在洁净空气中自动标定 R0。
- OneNET Studio 使用产品专属 TLS 域名和 `8883` 端口，不能把 `mqtts` 域名当作普通 TCP `1883` 使用。
- OneNET token 通常超过 ESP-AT `AT+MQTTUSERCFG` 的 password 长度限制，所以本工程使用 SSL socket + STM32 MQTT 组包。
- OneNET 属性和事件名称必须先在物模型中定义；平台会拒绝包含未知标识符的整个上报包。
