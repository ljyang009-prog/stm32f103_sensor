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

---

## 硬件组成

| 模块 | 说明 |
|------|------|
| 主控 | STM32F103RC（Mini STM32 开发板）|
| 温湿度 | DHT11 |
| 气体 | MQ-2（模拟量 + 数字量）|
| 光照 | 光敏电阻 + 补光 LED |
| 显示 | 2.4/2.8 寸 TFT LCD（ILI9341 等）|
| 通信 | ESP8266（WiFi，烧录 Arduino 固件）|
| 云平台 | OneNET Studio（物模型 / MQTT）|

---

## 引脚分配

### GPIOA
| 引脚 | 功能 | 说明 |
|------|------|------|
| PA0 | ESP8266 RST | 可选复位脚（Arduino 固件下未使用）|
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

采用「双芯片分工」架构：

```
┌──────────┐  USART2(PA2)   ┌─────────────────────┐  WiFi(TLS)  ┌────────────┐
│  STM32   │ ──────────────► │  ESP8266            │ ───────────► │  OneNET    │
│ 读传感器  │  每5秒发一行数据  │  (Arduino 固件)      │   MQTT 上报   │  Studio    │
│ LCD显示  │ "温度,湿度,气体,光照"│  连WiFi + MQTT(TLS) │              │  物模型     │
└──────────┘                 └─────────────────────┘             └────────────┘
```

- **STM32**（Keil 工程）：负责采集传感器、LCD 显示、报警，每 5 秒通过 USART2 发送一行数据 `温度,湿度,气体浓度,光照`（如 `24,53,5.4,81`）给 ESP8266。
- **ESP8266**（Arduino 固件）：自己连接 WiFi、通过 MQTT over TLS 连接 OneNET，收到 STM32 的数据后打包成物模型 JSON 上报。

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
├── SYSTEM/             # delay / sys / usart
├── USER/               # main.c + Keil 工程（ADC.uvprojx）
├── USMART/             # 串口调试组件
├── ESP8266_Arduino/    # ESP8266 的 Arduino 固件（.ino）
├── 原理图及引脚分配/     # 原理图与引脚分配表
└── README.md
```

---

## 编译与烧录

### 1. STM32（Keil MDK5）

1. 用 Keil MDK5 打开 `USER/ADC.uvprojx`。
2. 编译（ARM Compiler V5，器件 `STM32F103RC`）。
3. 用 ST-Link 等烧录生成的目标文件。

### 2. ESP8266（Arduino IDE）

1. 安装 Arduino IDE，添加 ESP8266 开发板支持：
   - 附加开发板管理器网址：`http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - 开发板管理器安装 `esp8266`
2. 库管理器安装 `PubSubClient`。
3. 打开 `ESP8266_Arduino/esp8266_onenet/esp8266_onenet.ino`。
4. 用 USB 转串口模块烧录（烧录时 GPIO0 接地进入烧录模式）。
5. 烧录完成后把 ESP8266 接回 STM32 的 USART2（PA2/PA3）。

---

## 配置说明

### ESP8266 固件配置（`ESP8266_Arduino/.../esp8266_onenet.ino`）

```cpp
const char* WIFI_SSID   = "你的WiFi名称";
const char* WIFI_PASS   = "你的WiFi密码";

const char* ONENET_PROID   = "产品ID";
const char* ONENET_DEVNAME = "设备名称";
const char* ONENET_TOKEN   = "生成的token";
```

### OneNET 平台配置

1. 登录 [OneNET Studio](https://open.iot.10086.cn) → 创建产品（MQTT 协议）。
2. 添加设备，得到 **产品ID**、**设备名称**、**设备密钥/access_key**（或生成 token）。
3. 配置物模型，添加 4 个属性：

| 功能名称 | 标识符 | 数据类型 | 单位 |
|---------|--------|---------|------|
| 温度 | `temperature` | double | ℃ |
| 湿度 | `humidity` | double | % |
| 有害气体浓度 | `gas` | double | ppm |
| 光照亮度 | `light` | int32 | % |

4. 上报 topic：`$sys/{产品ID}/{设备名称}/thing/property/post`

---

## 数据流（OneNET）

设备每 5 秒上报一次，4 个数据流：

| 数据流 | 含义 |
|--------|------|
| `temperature` | 温度 (℃) |
| `humidity` | 湿度 (%) |
| `gas` | 有害气体浓度 (ppm，由 MQ-2 电压换算) |
| `light` | 光照亮度 (%) |

---

## 说明

- MQ-2 的 ppm 浓度是基于 `Rs/R0` 曲线拟合的**近似值**（未用标准气体标定），仅作趋势参考；上电时会在洁净空气中自动标定 R0。
- 新版 OneNET Studio 的 MQTT 为 **TLS 加密**，普通 ESP8266 AT 固件不支持，因此本项目将 ESP8266 刷成 **Arduino 固件**自行完成 MQTT(TLS) 上报。
