<div align="center">
<img src="https://avatars.githubusercontent.com/u/245985800?s=200&v=4" style="width:100px;" width="100"/>
<h2>FasterEdge MCU - STC8</h2>
<h3>FasterEdge 框架的 STC8（8051 增强） 平台实现（Keil C51 / PlatformIO 版）</h3>
</div>

### 一、简介

本项目是 **[FasterEdge](https://github.com/FasterEdge/FasterEdge)** 框架在 ****STC8（8051 增强 1T 内核）****平台上的实现。STC8 为 1T 增强 8051：62KB Flash、8KB XRAM、内置 IAP/EEPROM，无网络、无操作系统，因此按 [MCU-C51](../MCU-C51) 的无网络精简思路裁剪能力子集，并保留 **寄存器 / GPIO / 芯片信息** 三个 MCU 专有模块。

- ✅ **keil/（Keil C51）** + **platformio_ide/（SDCC，PlatformIO）** 双版本
- ✅ 与主仓库**同名同命令**，云边协同对等编程
- ✅ HMAC-SHA256 纯 C 零依赖
- ✅ 配置/密钥持久化到内置 IAP/EEPROM 区
- ✅ platformio_ide 版为 **8051 寄存器级实现**（UART / 定时器 / 端口）

### 二、已实现能力（无网络合理子集）

**Ability（8 个）**

| 名称 | 类别 | 命令 |
|------|------|------|
| `BaseAbility` | 基础 | `list_data_names` / `list_ability_names` |
| `RoleAbility` | 角色 | `describe` / `set_role` / `get_role` |
| `TimeAbility` | 时间 | `sync_manual` / `sync_system` / `get_time` / `configure_run`（无 NTP）|
| `OneKeyAbility` | 令牌 | `issue_token` / `verify_token` / `revoke_all` / `list_tokens` / `status` / `rotate`（HMAC-SHA256）|
| `SerialAbility` | 串口 | `open` / `close` / `write` / `read` / `is_open` / `set_config` / `get_config` / `list_ports` |
| `ModbusAbility` | Modbus | `set_unit_id` / `get_unit_id` / `read_holding` / `read_input` / `read_coils` / `read_discrete` / `write_holding` / `write_coil`（RTU 从站）|
| `RegAbility` | 寄存器(专有) | `read <addr>` / `write <addr>,<value>` / `bit_set <addr>,<bit>` / `bit_clear <addr>,<bit>` / `info` |
| `GpioAbility` | GPIO(专有) | `mode <pin>,<input\|output\|input_pullup>` / `write <pin>,<0\|1>` / `read <pin>` / `info` |

**Data（3 个）**

| 名称 | 功能 | 命令 |
|------|------|------|
| `BaseData` | 框架元信息 | `logo` / `info` |
| `ConfigData` | KV 配置（EEPROM 持久化）| `get` / `set` / `delete` / `list` / `snapshot` |
| `ChipData` | 芯片信息(专有) | `info` |

### 三、排除项与理由

| 能力 | 排除原因 |
|------|---------|
| MQTTAbility / NetMapData | STC8 无网络协议栈 |
| EdgeRoleAbility | 依赖网络心跳上报 |
| ConfigFileAbility | 与 ConfigData 重复，且无文件系统概念 |
| KeyringData | 与 OneKeyAbility 合并（同一 EEPROM 密钥存储）|
| TimeAbility.sync_ntp | 无网络无法 SNTP 校时 |

### 四、目录结构

```
MCU-STC8/
├── keil/                       # Keil C51 版（uVision 工程）
│   ├── MDK-ARM/                # FasterEdge-MCU-STC8.uvproj（Keil C51）
│   ├── Core/                   # fe.h / fe.c / fe_hmac_sha256.c
│   ├── Inc/                    # fe_ability.h / fe_data.h / fe_port.h
│   ├── Ability/                # ability_*.c（8 个）
│   ├── Data/                   # data_*.c（3 个）
│   └── User/                   # main.c / register.c / fe_port.c（移植层）
└── platformio_ide/             # VS Code + PlatformIO IDE 工程（STC 平台 + SDCC）
    ├── platformio.ini          # board = stc8h8k64u
    ├── .vscode/extensions.json # 推荐 PlatformIO IDE 插件
    └── src/                    # 复用 keil 裸机 C + SDCC 版 fe_port（8051 寄存器级）
```

> 两套均为裸机 C 工具链：`keil/`（Keil C51 官方编译器）与 `platformio_ide/`（SDCC，VS Code 插件），能力与命令完全一致；STC-ISP 仅用于烧录。

### 五、使用说明

1. **keil 版**：用 Keil C51（uVision）打开 `keil/MDK-ARM/FasterEdge-MCU-STC8.uvproj`，编译生成 HEX，用 STC-ISP 烧录
2. **platformio_ide 版**：VS Code 安装 **PlatformIO IDE** 插件，打开 `platformio_ide/`，Build / Upload / Serial Monitor
3. 在 `fe_port.c` 中按你的 STC8 型号调整 UART/定时器（已附 STC8 参考片段）

**串口命令示例：**

```
help
ability_BaseAbility list_ability_names
ability_RoleAbility set_role edge
ability_TimeAbility sync_manual 1700000000
ability_OneKeyAbility issue_token sensor01
ability_ModbusAbility set_unit_id 3
ability_ModbusAbility write_holding 0,42
ability_ModbusAbility read_holding 0,4
ability_SerialAbility set_config 0,9600
ability_SerialAbility write hello
data_ConfigData set wifi.ssid=MyNet
data_ConfigData get wifi.ssid
data_BaseData info
```

### 六、平台适配要点

| 差异点 | ESP32/ESP8266 | STC8 |
|--------|--------------|---------------------|
| 架构 | Xtensa 32 位 | **MCS-51 (1T 增强)** |
| RAM / Flash | KB~MB | **8KB XRAM / 62KB Flash** |
| 存储 | NVS / Flash | **内置 IAP/EEPROM（DataFlash）** |
| 网络 | 有 | **无**（能力子集剔除网络项）|
| 寄存器 | 32 位内存映射 | **SFR 0x80-0xFF + XRAM 0x0000-0xFFFF（RegAbility 宽度 8）** |

### 七、platformio_ide 版实现说明（SDCC）

`platformio_ide/` 为 **SDCC 编译器版** 工程（PlatformIO STC 平台），复用 keil 版 C 代码，`fe_port.c` 为 SDCC 兼容的 8051 寄存器级实现（1T 增强：定时器 0 波特率 / 串口轮询收发 / IAP EEPROM 参考）。

```bash
cd platformio_ide
pio run            # 编译
pio run -t upload  # 烧录
pio device monitor # 串口监视（115200）
```

> 换型号：编辑 `platformio.ini` 的 `board`（如 `stc15f2k60s2`）；需要 `%lu` 长格式化时放开 `-Dprintf=printf_large`。

### 八、MCU 专有模块

除主仓库对应能力外，本仓库提供 3 个 **MCU 专有** 模块（寄存器 / GPIO / 芯片信息）。STC8 的寄存器操作针对 8051 双地址空间：**SFR**（特殊功能寄存器 0x80-0xFF）与 **XRAM**（扩展 RAM），端口为 P0-P3：

| 模块 | 类型 | 命令 | 说明 |
|------|------|------|------|
| RegAbility | Ability | `read <addr>` / `write <addr>,<value>` / `bit_set <addr>,<bit>` / `bit_clear <addr>,<bit>` / `info` | 8051 SFR / XRAM 读写（fe_port 跳转表 + xdata 指针） |
| GpioAbility | Ability | `mode <pin>,<input\|output\|input_pullup>` / `write <pin>,<0\|1>` / `read <pin>` / `info` | 8051 端口 P0-P3（port 0-3，寄存器级） |
| ChipData | Data | `info` | STC8 型号 / RAM / Flash / EEPROM / 频率 |

**示例：**

```
ability_RegAbility read_sfr 0x90        # 读 P1
ability_RegAbility write_sfr 0x90,0xAA  # 写 P1
ability_RegAbility read_xram 0x1234
ability_RegAbility write_xram 0x1234,0x55
ability_GpioAbility write 0,0xAA        # 写 P0
ability_GpioAbility read 1
data_ChipData info
```

> ⚠️ 寄存器操作直接访问硬件，误写可能导致系统异常，仅供调试/底层驱动使用。

### 九、与 FasterEdge 主仓库的对应关系

- 命令名与主仓库**完全一致**，与 MCU-C51 / MCU-ESP32 实现同构
- `Atom` 模型：单例全局 Atom，`data_` / `ability_` 前缀路由
- 令牌用 HMAC-SHA256（纯 C，无 mbedTLS），密钥 EEPROM 持久化
- Modbus 寄存器表存 RAM，RTU 帧服务入口 `modbus_slave_service()` 已预留

### 十、姊妹项目

- **[FasterEdge MCU - ESP32](https://github.com/FasterEdge/MCU-ESP32)**：双核、WiFi/BLE、更多外设
- **[FasterEdge MCU - ESP8266](https://github.com/FasterEdge/MCU-ESP8266)**：WiFi、低功耗
- **[FasterEdge MCU - C51](https://github.com/FasterEdge/MCU-C51)**：8 位 8051，最精简
- **[FasterEdge MCU - Arduino Uno R3](https://github.com/FasterEdge/MCU-Arduino-Uno-R3)**：8 位 AVR（ATmega328P）
- **[FasterEdge MCU - Arduino Uno R4](https://github.com/FasterEdge/MCU-Arduino-Uno-R4)**：32 位 Cortex-M4F（RA4M1）
- **[FasterEdge](https://github.com/FasterEdge/FasterEdge)**：框架主仓库
