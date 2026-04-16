# AGENTS.md - ESP32 Blackbox Project

ESP32-C3 network probing device with AP config portal. Monitors endpoint availability, exposes Prometheus metrics on port 9090.

**Tech Stack**: ESP-IDF v6.0, C (gnu23), FreeRTOS, LWIP, mbedTLS v4 (PSA Crypto)
**Target**: ESP32-C3-Mini (RISC-V, 4MB Flash)
**Commit**: `28b4877` | **Branch**: `main`

## Build Commands

```bash
idf.py set-target esp32c3          # Set target (first time only)
idf.py menuconfig                  # Configure AP SSID/password, retry count
idf.py build                       # Compile
idf.py -p COM3 flash               # Flash (Windows)
idf.py -p COM3 flash monitor       # Flash + serial monitor (Ctrl+] to exit)
idf.py fullclean                   # Clean all build artifacts
```

**No tests.** No test framework configured.

## Structure

```
esp32-blackbox/
├── main/                          # Application source (see main/AGENTS.md)
├── components/json/               # Vendored cJSON (ESP-IDF v6.0 removed built-in json)
├── docs/                          # Architecture, design, usage docs
├── CMakeLists.txt                 # Root project config
├── sdkconfig.defaults             # Default sdkconfig values
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

### Adding a New Probe Type
1. Add enum to `probe_type_t` in `config_manager.h`
2. Create `probe_xxx.c` with execute function
3. Add signature to `probe_types.h`
4. Register in `probe_manager.c` switch
5. Add to `main/CMakeLists.txt` SRCS + update `metrics_server.c` type string map

## Configuration

| Method | What | Where |
|--------|------|-------|
| NVS (runtime) | WiFi SSID/password | Saved via AP config portal |
| Kconfig (build) | AP SSID/password, retry count | `idf.py menuconfig` → ESP32 Blackbox Configuration |
| Code (build) | Probe targets, metrics port | `main/config_manager.c` `s_targets[]` |
| Defaults | Base sdkconfig | `sdkconfig.defaults` |

## Important Notes

- **ESP-IDF v6.0**: mbedTLS v4 removed `entropy.h`, `ctr_drbg.h`, `mbedtls_ssl_conf_rng()` — TLS uses PSA Crypto internally
- **Local json component**: `components/json/` is vendored cJSON (v6.0 dropped built-in `json` component)
- **No dynamic allocation in probe hot paths** — use stack/structs
- **App partition 94% full** — adding features requires partition table enlargement or debug log cleanup
- **FreeRTOS stacks**: probe=16KB, metrics=8KB, config_server=4KB
- **Minimum probe interval**: 5000ms (lower risks watchdog timeout)
- **WiFi auto-reconnect**: max 5 retries, then stays disconnected
- **AP config portal**: SSID `ESP32_Blackbox`, password `12345678`, IP `192.168.4.1`
