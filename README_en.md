<div align="center">
<img src="https://avatars.githubusercontent.com/u/245985800?s=200&v=4" style="width:100px;" width="100"/>
<h2>FasterEdge MCU - STC8</h2>
<h3>FasterEdge framework on STC8 (8051 enhanced) (Keil C51 / PlatformIO editions)</h3>
</div>

### 1. Introduction

This repo implements the **[FasterEdge](https://github.com/FasterEdge/FasterEdge)** framework on the **STC8 (8051 enhanced 1T core)**. STC8 is a 1T enhanced 8051: 62KB Flash, 8KB XRAM, built-in IAP/EEPROM — no network, no OS. Following the [MCU-C51](../MCU-C51) no-network design, the capability set is trimmed and 3 **MCU-specific** modules (registers / GPIO / chip info) are kept.

- ✅ **keil/ (Keil C51)** + **platformio_ide/ (SDCC, PlatformIO)** dual editions
- ✅ Same names & commands as the main repo — peer programming for edge/cloud
- ✅ HMAC-SHA256 in pure C (zero dependencies)
- ✅ Config/keys persisted to the built-in IAP/EEPROM area
- ✅ platformio_ide edition ships **8051 register-level drivers** (UART / timer / ports)

### 2. Implemented Capabilities (no-network subset)

**Abilities (8)**

| Name | Type | Commands |
|------|------|----------|
| `BaseAbility` | Base | `list_data_names` / `list_ability_names` |
| `RoleAbility` | Role | `describe` / `set_role` / `get_role` |
| `TimeAbility` | Time | `sync_manual` / `sync_system` / `get_time` / `configure_run` (no NTP) |
| `OneKeyAbility` | Token | `issue_token` / `verify_token` / `revoke_all` / `list_tokens` / `status` / `rotate` (HMAC-SHA256) |
| `SerialAbility` | Serial | `open` / `close` / `write` / `read` / `is_open` / `set_config` / `get_config` / `list_ports` |
| `ModbusAbility` | Modbus | `set_unit_id` / `get_unit_id` / `read_holding` / `read_input` / `read_coils` / `read_discrete` / `write_holding` / `write_coil` (RTU slave) |
| `RegAbility` | Reg (own) | `read <addr>` / `write <addr>,<value>` / `bit_set <addr>,<bit>` / `bit_clear <addr>,<bit>` / `info` |
| `GpioAbility` | GPIO (own) | `mode <pin>,<input|output|input_pullup>` / `write <pin>,<0|1>` / `read <pin>` / `info` |

**Data (3)**

| Name | Type | Commands |
|------|------|----------|
| `BaseData` | Meta | `logo` / `info` |
| `ConfigData` | KV config (EEPROM) | `get` / `set` / `delete` / `list` / `snapshot` |
| `ChipData` | Chip info (own) | `info` |

### 3. Excluded Capabilities

| Capability | Reason |
|------------|--------|
| MQTTAbility / NetMapData | No network stack on STC8 |
| EdgeRoleAbility | Needs network heartbeat |
| ConfigFileAbility | Redundant with ConfigData; no filesystem concept |
| KeyringData | Merged into OneKeyAbility (same EEPROM key) |
| TimeAbility.sync_ntp | No network for SNTP |

### 4. Directory Layout

```
MCU-STC8/
├── keil/                       # Keil C51 edition (uVision project)
│   ├── MDK-ARM/                # FasterEdge-MCU-STC8.uvproj (Keil C51)
│   ├── Core/                   # fe.h / fe.c / fe_hmac_sha256.c
│   ├── Inc/                    # fe_ability.h / fe_data.h / fe_port.h
│   ├── Ability/                # ability_*.c (8)
│   ├── Data/                   # data_*.c (3)
│   └── User/                   # main.c / register.c / fe_port.c (porting layer)
└── platformio_ide/             # VS Code + PlatformIO IDE project (STC platform + SDCC)
    ├── platformio.ini          # board = stc8h8k64u
    ├── .vscode/extensions.json # recommends PlatformIO IDE
    └── src/                    # keil C + SDCC fe_port (8051 register-level)
```

> Both are bare-metal C toolchains: `keil/` (official Keil C51) and `platformio_ide/` (SDCC, VS Code plugin), identical capabilities & commands; STC-ISP is for flashing only.

### 5. Usage

1. **keil edition**: open `keil/MDK-ARM/FasterEdge-MCU-STC8.uvproj` in Keil C51 (uVision), build HEX, flash via STC-ISP
2. **platformio_ide edition**: install the **PlatformIO IDE** VS Code extension, open `platformio_ide/`, Build / Upload / Serial Monitor
3. Adjust UART/timer in `fe_port.c` for your STC8 model (STC8 reference snippets included)

**Serial command examples:**

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

### 6. Platform Differences

| Aspect | ESP32/ESP8266 | STC8 |
|--------|---------------|---------------------|
| Architecture | Xtensa 32-bit | **MCS-51 (1T enhanced)** |
| RAM / Flash | KB~MB | **8KB XRAM / 62KB Flash** |
| Storage | NVS / Flash | **Built-in IAP/EEPROM (DataFlash)** |
| Network | Yes | **No** (network items trimmed) |
| Registers | 32-bit MMIO | **SFR 0x80-0xFF + XRAM 0x0000-0xFFFF (RegAbility width 8)** |

### 7. platformio_ide Notes (SDCC)

The `platformio_ide/` edition is the **SDCC** project (PlatformIO STC platform), reusing the keil C code; `fe_port.c` is an SDCC-compatible 8051 register-level implementation (1T enhanced: Timer0 baud / polled UART / IAP EEPROM reference).

```bash
cd platformio_ide
pio run            # build
pio run -t upload  # flash
pio device monitor # serial monitor (115200)
```

> To change MCU: edit `board` in `platformio.ini` (e.g. `stc15f2k60s2`); enable `-Dprintf=printf_large` for `%lu` formatting.

### 8. MCU-Specific Modules

Beyond main-repo capabilities, 3 **MCU-specific** modules (registers / GPIO / chip info) are provided. STC8 register access targets the 8051 dual address space: **SFR** (special function registers 0x80-0xFF) and **XRAM** (external RAM), ports P0-P3:

| Module | Type | Commands | Description |
|--------|------|----------|-------------|
| RegAbility | Ability | `read <addr>` / `write <addr>,<value>` / `bit_set <addr>,<bit>` / `bit_clear <addr>,<bit>` / `info` | 8051 SFR / XRAM read-write (fe_port jump table + xdata pointer) |
| GpioAbility | Ability | `mode <pin>,<input|output|input_pullup>` / `write <pin>,<0|1>` / `read <pin>` / `info` | 8051 ports P0-P3 (port 0-3, register-level) |
| ChipData | Data | `info` | STC8 model / RAM / Flash / EEPROM / freq |

**Examples:**

```
ability_RegAbility read_sfr 0x90        # read P1
ability_RegAbility write_sfr 0x90,0xAA  # write P1
ability_RegAbility read_xram 0x1234
ability_RegAbility write_xram 0x1234,0x55
ability_GpioAbility write 0,0xAA        # write P0
ability_GpioAbility read 1
data_ChipData info
```

> ⚠️ Register access touches hardware directly; a wrong write may crash the system. Debug/low-level use only.

### 9. Correspondence with the Main Repo

- Commands match the main repo exactly, and the implementation is isomorphic with MCU-C51 / MCU-ESP32.
- `Atom` model: singleton global Atom, `data_` / `ability_` prefix routing.
- Tokens via HMAC-SHA256 (pure C, no mbedTLS), key persisted in EEPROM.
- Modbus register tables live in RAM; RTU entry `modbus_slave_service()` is reserved.

### 10. Sibling Projects

- **[FasterEdge MCU - ESP32](https://github.com/FasterEdge/MCU-ESP32)**: dual-core, WiFi/BLE, more peripherals
- **[FasterEdge MCU - ESP8266](https://github.com/FasterEdge/MCU-ESP8266)**: WiFi, low power
- **[FasterEdge MCU - C51](https://github.com/FasterEdge/MCU-C51)**: 8-bit 8051, most minimal
- **[FasterEdge MCU - Arduino Uno R3](https://github.com/FasterEdge/MCU-Arduino-Uno-R3)**: 8-bit AVR (ATmega328P)
- **[FasterEdge MCU - Arduino Uno R4](https://github.com/FasterEdge/MCU-Arduino-Uno-R4)**: 32-bit Cortex-M4F (RA4M1)
- **[FasterEdge](https://github.com/FasterEdge/FasterEdge)**: framework main repo
