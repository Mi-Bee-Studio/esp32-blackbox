# AGENTS.md - ESP32 Blackbox Project

ESP32 network probing device with AP config portal. Monitors endpoint availability, exposes Prometheus metrics on port 9090. 现支持即席探测功能 `/probe`。
**Tech Stack**: ESP-IDF v6.0, C (gnu23), FreeRTOS, LWIP, mbedTLS v4 (PSA Crypto)
**Targets**: ESP32-C3 SuperMini | Seeed Studio XIAO ESP32C6
**Commit**: `28b4877` | **Branch**: `main`

## Supported Boards

| Board | Chip | Arch | SRAM | Flash | WiFi | Special |
|-------|------|------|------|-------|------|---------|
| ESP32-C3 SuperMini (Nologo) | ESP32-C3 | RISC-V single-core 160MHz | 400KB | 4MB (embedded) | 802.11 b/g/n | - |
| Seeed Studio XIAO ESP32C6 | ESP32-C6 | RISC-V dual-core (HP 160MHz + LP 20MHz) | 512KB | 4MB (external) | 802.11ax (WiFi 6) | BLE 5.3, Zigbee, Thread |

## Build Commands

### Prerequisites

1. Install ESP-IDF v6.0: https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c6/get-started/
2. Set up environment (one of):
   - Source `export.sh` / `export.bat` (puts `idf.py` in PATH)
   - Set `IDF_PATH` environment variable
   - Install to standard location (`~/esp/esp-idf`)

### 方式一：Python 构建脚本（推荐，跨平台）

```bash
# ESP32-C6 构建
python build.py esp32c6 build

# ESP32-C6 构建并烧录
python build.py esp32c6 flash COM3

# ESP32-C6 全量清理后构建
python build.py esp32c6 clean

# ESP32-C3 构建并烧录+监控
python build.py esp32c3 monitor COM3
```

### 方式二：ESP-IDF 命令行（需先运行 export.bat / source export.sh）

```bash
# 首次构建 - 设置目标芯片
idf.py set-target esp32c6        # 或 esp32c3

# 配置（可选）
idf.py menuconfig                # 配置 AP SSID/password, 重试次数, 自检开关

# 编译
idf.py build

# 烧录
idf.py -p COM3 flash

# 烧录 + 串口监控
idf.py -p COM3 flash monitor     # Ctrl+] 退出监控

# 清理
idf.py fullclean                  # 清理所有构建产物
```

### GitHub Releases（预编译固件）

每次发布版本（tag push `v*`）会自动构建 ESP32-C3 和 ESP32-C6 固件并发布到 GitHub Releases 页面。

下载地址: https://github.com/<user>/esp32-blackbox/releases

烧录命令:
```bash
# ESP32-C3
esptool.py --chip esp32c3 -p COM3 -b 460800 write_flash \
  0x0 bootloader-esp32c3.bin \
  0x8000 partition-table-esp32c3.bin \
  0x10000 esp32-blackbox-esp32c3.bin

# ESP32-C6
esptool.py --chip esp32c6 -p COM3 -b 460800 write_flash \
  0x0 bootloader-esp32c6.bin \
  0x8000 partition-table-esp32c6.bin \
  0x10000 esp32-blackbox-esp32c6.bin
```

### 切换目标芯片

从 C3 切换到 C6（或反之）时，必须重新设置目标：

```bash
idf.py set-target esp32c6    # 自动执行 fullclean + 重新生成 sdkconfig
```

ESP-IDF 会自动加载对应的 `sdkconfig.defaults.<target>` 文件。

## Board Self-Test (硬件自检)

启动时自动运行 7 项硬件自检，验证板卡功能：

| # | 测试项 | 验证内容 |
|---|--------|----------|
| 1 | NVS 存储 | 写入→读回→验证→清除 |
| 2 | WiFi 初始化 | netif + WiFi 驱动 + MAC 地址 |
| 3 | WiFi 扫描 | STA 模式扫描周围 AP |
| 4 | DNS 解析 | 解析 dns.google |
| 5 | HTTP 探测 | 请求 httpbin.org |
| 6 | TCP 探测 | 连接 8.8.8.8:53 |
| 7 | 指标服务器 | 端口 9090 绑定 |

**默认行为**：
- ESP32-C6: 自检默认**开启** (`CONFIG_ESP_BOARD_TEST=y`)
- ESP32-C3: 自检默认**关闭** (`CONFIG_ESP_BOARD_TEST=n`)

通过 `idf.py menuconfig` → ESP32 Blackbox Configuration → Run board self-test on startup 调整。

## Structure

```
esp32-blackbox/
├── main/                          # Application source (see main/AGENTS.md)
│   ├── status_led.c              # LED 状态指示模块（C3/C6 GPIO 控制）
│   ├── status_led.h              # LED 状态 API 和状态枚举
├── components/json/               # Vendored cJSON (ESP-IDF v6.0 removed built-in json)
├── docs/                          # Documentation directory
│   ├── zh/                        # Chinese documentation
│   │   ├── architecture.md        # 架构设计
│   │   ├── design.md              # 详细设计
│   │   └── usage.md               # 使用教程
│   └── en/                        # English documentation
│       ├── architecture.md        # Architecture
│       ├── design.md              # Design
│       └── usage.md               # Usage Guide
├── CMakeLists.txt                 # Root project config
├── sdkconfig.defaults             # Common default config (all targets)
├── sdkconfig.defaults.esp32c3     # ESP32-C3 specific config (flash, crypto)
├── partitions.csv                 # Custom partition table (1.875MB app for C6)
├── build.py                       # Python build script (cross-target)
└── AGENTS.md                      # This file
```

## Code Style

- **4-space indent**, braces on same line, 120 char max
- **No .clang-format / .editorconfig** — uses ESP-IDF defaults
- **All comments in Chinese** — code comments are bilingual (Chinese descriptions)
- **Static globals**: `s_` prefix (e.g., `s_state`, `s_probe_task_handle`)
- **Log TAGs**: Uppercase module abbreviation (e.g., `TAG = "WIFI"`, `TAG = "PROBE_MGR"`)

### Naming

| Type | Convention | Example |
|------|------------|---------|
| Files | `snake_case` | `wifi_manager.c` |
| Functions | `snake_case` | `wifi_manager_init()` |
| Types | `snake_case_t` | `probe_result_t` |
| Macros/Enums | `SCREAMING_SNAKE_CASE` | `PROBE_TYPE_HTTP` |

### Include Order

```c
#include <stdio.h>              // 1. System (angle brackets)
#include "freertos/FreeRTOS.h"  // 2. ESP-IDF (quotes)
#include "wifi_manager.h"       // 3. Local project (quotes)
```

### Error Handling

Two patterns:
- **ESP-IDF style**: `esp_err_t ret = func(); if (ret != ESP_OK) { ESP_LOGE(...); return ret; }`
- **Result struct style**: `probe_result_t result = {0}; ... strncpy(result.error_msg, ...); return result;`

## Key Patterns

### Static Module Pattern
Every module follows this:
```c
static const char *TAG = "MODULE";
static module_state_t s_state;

esp_err_t module_init(void) {
    ESP_LOGI(TAG, "Module initialized");
    return ESP_OK;
}
```

### JSON Config Pattern
SPIFFS-based JSON configuration system:
```c
/* Mount SPIFFS */
esp_err_t ret = esp_vfs_spiffs_mount("/spiffs", NULL, &mount_config);
/* Load blackbox.json */
blackbox_config_t *config = config_load_json();
/* Check for hot-load */
if (config_get_version() != s_config_version) {
    config_reload();
    s_config_version = config_get_version();
}
```

### Module System Pattern
Probe modules use named configuration:
```c
typedef struct {
    char name[32];                    /* Module name */
    probe_module_config_t config;    /* Module-specific config */
} probe_module_t;

/* Target references module by name */
typedef struct {
    char name[64];                    /* Target name */
    char module_name[32];            /* Module name (not type) */
    char target[256];                /* Hostname/IP */
    uint16_t port;                  /* Port */
    uint32_t interval_ms;           /* Interval */
} probe_target_t;
```

### Config Hot-Load Pattern
Configuration version tracking for runtime updates:
```c
static uint8_t s_config_version = 0;

/* Check for config updates each probe loop */
void probe_manager_loop(void) {
    if (config_get_version() != s_config_version) {
        probe_manager_reload_targets();
        s_config_version = config_get_version();
    }
    /* ... rest of probe logic ... */
}
```

### Adding a New Probe Type
1. Add probe module type to `probe_module_type_t` enum in `config_manager.h`
2. Add module config structure to `module_config_union_t` in `config_manager.h`
3. Create `probe_xxx.c` with execute function
4. Add signature to `probe_types.h`
5. Add to factory defaults in `config_manager.c` `s_factory_modules[]`
6. Register in `probe_manager.c` switch
7. Add to `main/CMakeLists.txt` SRCS + update `metrics_server.c` type string map

### Adding a New Target Board
1. Create `sdkconfig.defaults.<target>` with flash/crypto config
2. Add `#elif CONFIG_IDF_TARGET_<TARGET>` in `main.c` `print_board_info()`
3. Add target-specific default in `Kconfig.projbuild` for `ESP_BOARD_TEST`
4. Test with `python build.py <target> clean`

### LED Status Indicator Pattern
- State enum `status_led_state_t` with 8 states
- `status_led_init()` initializes GPIO, creates LED task, registers WiFi/IP event handlers
- LED task runs in a loop reading current state and driving blink patterns
- Kconfig `CONFIG_ESP_STATUS_LED` with inline stubs when disabled
- Module registers its OWN event handlers (does not modify wifi_manager.h)
- Static allocation only, no malloc in LED hot paths

#### LED State Table
| State | Pattern |
|-------|---------|
| INIT | Fast blink 100ms |
| AP_MODE | Slow blink 500ms |
| STA_CONNECTING | Medium blink 200ms |
| CONNECTED | Solid ON |
| DISCONNECTED | Fast blink 100ms |
| CONNECTION_FAILED | 3x blink + 1s pause |
| SELF_TEST | Fast blink 100ms |
| CONFIG_RELOAD | 2x blink |

## Configuration
| Method | What | Where |
|--------|------|-------|
| NVS (runtime) | WiFi SSID/password | Saved via AP config portal |
| Kconfig (build) | AP SSID/password, retry count, self-test, LED 状态指示开关, GPIO 引脚 | `idf.py menuconfig` → ESP32 Blackbox Configuration |
| JSON config (SPIFFS) | Probe modules, targets, intervals, metrics port | `/spiffs/blackbox.json` (hot-loadable) |
| Code (build) | Factory defaults for modules | `main/config_manager.c` `s_factory_modules[]` |
| Code (build) | NVS WiFi credentials storage | `main/config_manager.c` `config_manager_*` APIs |
| sdkconfig.defaults | Common settings | `sdkconfig.defaults` |
| sdkconfig.defaults.<target> | Target-specific (flash, crypto) | Auto-loaded by ESP-IDF build system |
## Important Notes

- **ESP-IDF v6.0**: mbedTLS v4 removed `entropy.h`, `ctr_drbg.h`, `mbedtls_ssl_conf_rng()` — TLS uses PSA Crypto internally
- **Local json component**: `components/json/` is vendored cJSON (v6.0 dropped built-in `json` component)
- **No dynamic allocation in probe hot paths** — use stack/structs
- **FreeRTOS stacks**: probe=16KB, metrics=8KB, config_server=4KB
- **Minimum probe interval**: 5000ms (lower risks watchdog timeout)
- **WiFi auto-reconnect**: max 5 retries, then stays disconnected
- **AP config portal**: SSID `ESP32_Blackbox`, password `12345678`, IP `192.168.4.1`
- **Multi-target**: Source code is target-agnostic (no GPIO/UART/SPI). All hardware differences handled by sdkconfig defaults
- **XIAO ESP32C6 Flash**: Some batches have 8MB flash — edit `sdkconfig.defaults.esp32c6` if needed

## 即席探测功能

### 概述
ESP32 Blackbox 现支持即席探测功能，通过 `/probe` 端点可以直接进行网络连通性测试，无需修改配置文件。

### 使用方法

**基本语法**:
```bash
GET /probe?target=<主机名/IP>&module=<模块名>&port=<端口>
```

**参数说明**:
• `target`: 目标主机名或 IP 地址 (必需)
• `module`: 探测模块名称 (必需)
• `port`: 目标端口 (可选，默认由模块决定)

**支持模块**:
| 模块 | 协议 | 说明 |
------|------|------|
 `http_2xx` | HTTP/HTTPS | HTTP GET/POST 探测，支持状态码验证 |
 `tcp` | TCP | TCP 连接测试 |
 `dns` | DNS | DNS 解析测试 |
 `icmp_ping` | ICMP | ICMP Ping 测试 |
 `ws` | WebSocket | WebSocket 连接测试 |
 `wss` | WebSocket Secure | WebSocket + TLS 连接测试 |

**使用示例**:
```bash
#VY# HTTP 探测
curl "http://<设备IP>:9090/probe?target=httpbin.org&module=http_2xx"

# TCP 探测 (指定端口)
#TMcurl "http://<设备IP>:9090/probe?target=example.com&module=tcp&port=443"

# DNS 探测
curl "http://<设备IP>:9090/probe?target=8.8.8.8&module=dns"
```

### 输出格式

即席探测返回 Prometheus 格式的指标数据：
```text
# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
#SRprobe_duration_seconds{target="httpbin.org", module="http_2xx"} 0.234

 HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
#HNprobe_success{target="httpbin.org", module="http_2xx"} 1
```

### 集成到 Prometheus

可以通过配置 Prometheus Blackbox Exporter 来使用即席探测功能：
```yaml
scrape_configs:
  - job_name: 'custom_probes'
    metrics_path: /probe
    params:
      module: [http_2xx]
    static_configs:
      - targets:
          - your-service.com
    relabel_configs:
      - source_labels: [__address__]
        target_label: __param_target
      - source_labels: [__param_target]
        target_label: instance
      - target_label: __address__
        replacement: <设备IP>:9090
```

### 优势特点

✅ **无需配置文件**: 直接通过 URL 进行网络测试
✅ **快速验证**: 立即获得测试结果
✅ **灵活参数**: 支持任意主机名、IP 和端口
✅ **标准化输出**: Prometheus 格式，易于集成
✅ **调试友好**: 实时网络连通性检查

### 使用限制

• 建议探测间隔不少于 5 秒，避免过频繁调用
• 即席探测主要用于临时测试，长期监控建议使用配置文件
• TLS 探测会消耗较多资源，注意不要并发过多
• 超时时间默认为 10 秒，可根据网络环境调整
• 探测结果仅返回当前状态，不保存历史记录
