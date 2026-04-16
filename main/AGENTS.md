# AGENTS.md - main/

Application source. All modules follow the static module pattern with `s_` prefix globals and uppercase `TAG`.

## Module Map

| File | Role | Lines | Key exports |
|------|------|-------|-------------|
| `main.c` | Entry point (`app_main`), NVS init, service orchestration | ~80 | — |
| `wifi_manager.c` | WiFi AP/STA dual mode, event handler, NVS credential check | 254 | `wifi_manager_init()`, `wifi_manager_start()` |
| `wifi_manager.h` | WiFi manager API | ~30 | `wifi_manager_is_connected()` |
| `wifi_config_server.c` | AP mode HTTP server (port 80), WiFi scan, credential save | 229 | `wifi_config_server_start()` |
| `wifi_config_server.h` | Config server API | ~10 | — |
| `config_manager.c` | Probe target definitions (`s_targets[]`), metrics port | 142 | `config_manager_init()`, `config_manager_get_config()` |
| `config_manager.h` | Config types: `blackbox_config_t`, `probe_target_t`, `probe_type_t` | ~60 | All type definitions |
| `probe_manager.c` | Probe task scheduler, result storage, type dispatch | 108 | `probe_manager_init()`, `probe_manager_get_targets()`, `probe_manager_get_results()` |
| `probe_manager.h` | Probe manager API | ~15 | — |
| `probe_types.h` | Probe result struct (`probe_result_t`), function pointer typedef, all probe execute declarations | 66 | `probe_func_t`, `probe_result_t` |
| `probe_http.c` | HTTP/HTTPS via `esp_http_client` | 117 | `probe_http_execute()`, `probe_https_execute()` |
| `probe_tcp.c` | TCP connect + TLS handshake via mbedTLS v4 (PSA Crypto) | 166 | `probe_tcp_execute()`, `probe_tcp_tls_execute()` |
| `probe_dns.c` | DNS resolution via `lwip/netdb` | ~80 | `probe_dns_execute()` |
| `probe_ws.c` | WebSocket upgrade + WSS via mbedTLS v4 | 300 | `probe_ws_execute()`, `probe_wss_execute()` |
| `metrics_server.c` | Prometheus `/metrics` HTTP server on config port | 197 | `metrics_server_start()` |
| `metrics_server.h` | Metrics server API | ~10 | — |

## Dependency Graph

```
main.c
  ├─→ wifi_manager ──→ nvs_flash, esp_wifi, esp_event
  │       └─→ wifi_config_server ──→ esp_http_server, json
  ├─→ config_manager (static targets, no deps)
  ├─→ probe_manager
  │       ├─→ probe_http ──→ esp_http_client
  │       ├─→ probe_tcp ──→ lwip/sockets, mbedtls (ssl, net_sockets)
  │       ├─→ probe_dns ──→ lwip/netdb
  │       └─→ probe_ws ──→ lwip/sockets, mbedtls (ssl, net_sockets)
  └─→ metrics_server ──→ lwip/sockets, probe_manager, config_manager
```

## Startup Flow

```
app_main()
  → nvs_flash_init()
  → esp_event_loop_create_default()
  → config_manager_init()
  → wifi_manager_init()
      ├─ NVS has WiFi creds? → STA mode → wait for IP
      └─ No creds? → AP mode → wifi_config_server_start()
  → probe_manager_init() → probe_manager_start()
  → metrics_server_start()
```

## ESP-IDF v6.0 Gotchas

- **mbedTLS v4**: No `entropy.h`, no `ctr_drbg.h`, no `mbedtls_ssl_conf_rng()`. TLS uses PSA Crypto internally — just configure `mbedtls_ssl_config` and call `mbedtls_ssl_setup()`.
- **json component**: v6.0 removed built-in `json`. Project uses local `components/json/` (vendored cJSON). Referenced as `json` in CMakeLists.txt REQUIRES.
- **probe_tcp.c / probe_ws.c**: TLS sections were migrated from entropy+ctr_drbg pattern to PSA Crypto. Do NOT re-add entropy/ctr_drbg includes.

## Anti-Patterns (Do NOT)

- Do NOT use `as any`, `@ts-ignore` equivalent suppressions
- Do NOT use dynamic allocation (`malloc`/`calloc`) in probe execution paths
- Do NOT add `mbedtls_entropy_*` or `mbedtls_ctr_drbg_*` — removed in v6.0
- Do NOT use en-dash `–` (U+2013) in ESP_LOG format strings — breaks `esp_log_color.h` macro expansion
- Do NOT use `MAC2STR()` without `MACSTR` format placeholder in the same ESP_LOG call
- Do NOT put trailing whitespace inside string literals before `);` — causes "missing terminating character" errors

## Kconfig Options

Defined in `Kconfig.projbuild` under "ESP32 Blackbox Configuration":

| Option | Type | Default | Purpose |
|--------|------|---------|---------|
| `ESP_AP_SSID` | string | `ESP32_Blackbox` | AP config portal SSID |
| `ESP_AP_PASSWORD` | string | `12345678` | AP config portal password |
| `ESP_MAXIMUM_RETRY` | int | `5` | STA connection retry limit |
