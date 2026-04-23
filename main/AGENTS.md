# AGENTS.md - main/

Application source. All modules follow the static module pattern with `s_` prefix globals and uppercase `TAG`.

## Module Map

| File | Role | Lines | Key exports |
|------|------|-------|-------------|
| `main.c` | Entry point (`app_main`), NVS init, service orchestration | ~134 | — |
| `wifi_manager.c` | WiFi AP/STA dual mode, event handler, NVS credential check | 254 | `wifi_manager_init()`, `wifi_manager_start()` |
| `wifi_manager.h` | WiFi manager API | ~30 | `wifi_manager_is_connected()` |
| `config_manager.c` | SPIFFS mount, JSON config hot-load, probe modules & targets | ~984 | `config_manager_init()`, `config_get_modules()`, `config_get_targets()` |
| `config_manager.h` | Config types: `blackbox_config_t`, `probe_target_t`, `probe_module_type_t` | ~160 | All type definitions |
| `probe_manager.c` | Per-target scheduling, trigger_probe, config hot-load detection | ~235 | `probe_manager_init()`, `probe_manager_trigger_probe()` |
| `probe_manager.h` | Probe manager API | ~15 | — |
| `probe_types.h` | Probe result struct (`probe_result_t`), function pointer typedef, all probe execute declarations | 66 | `probe_func_t`, `probe_result_t` |
| `probe_http.c` | HTTP/HTTPS via `esp_http_client` | 117 | `probe_http_execute()`, `probe_https_execute()` |
| `probe_tcp.c` | TCP connect + TLS handshake via mbedTLS v4 (PSA Crypto) | 166 | `probe_tcp_execute()`, `probe_tcp_tls_execute()` |
| `probe_dns.c` | DNS resolution via `lwip/netdb` | ~80 | `probe_dns_execute()` |
| `probe_icmp.c` | ICMP ping via lwIP raw sockets | 261 | `probe_icmp_execute()` |
| `probe_ws.c` | WebSocket upgrade + WSS via mbedTLS v4 | 300 | `probe_ws_execute()`, `probe_wss_execute()` |
| `web_server.c` | Unified AP/STA web server (port 80), config dashboard | 597 | `web_server_start()`, `wifi_config_server_start()` |
| `web_server.h` | Web server API | ~10 | — |
| `metrics_server.c` | Prometheus HTTP server (esp_http_server), /metrics, /probe, /config, /reload | ~341 | `metrics_server_start()` |
| `metrics_server.h` | Metrics server API | ~10 | — |

## Dependency Graph

```
main.c
  ├─→ wifi_manager ──→ nvs_flash, esp_wifi, esp_event
  │       └─→ web_server ──→ esp_http_server, json
  ├─→ config_manager ──→ SPIFFS, json
  │       └─→ s_config_version (hot-load tracking)
  ├─→ probe_manager
  │       ├─→ probe_http ──→ esp_http_client
  │       ├─→ probe_tcp ──→ lwip/sockets, mbedtls (ssl, net_sockets)
  │       ├─→ probe_dns ──→ lwip/netdb
  │       ├─→ probe_icmp ──→ lwip/raw
  │       └─→ probe_ws ──→ lwip/sockets, mbedtls (ssl, net_sockets)
  └─→ metrics_server ──→ esp_http_server, probe_manager, config_manager
```

## Startup Flow

```
app_main()
  → nvs_flash_init()
  → esp_event_loop_create_default()
  → config_manager_init() [mounts SPIFFS, loads JSON config]
  → wifi_manager_init()
      ├─ NVS has WiFi creds? → STA mode → wait for IP
      │   → board_test (if enabled)
      │   → probe_manager_init() → probe_manager_start()
      │   → metrics_server_start() [port 9090: /metrics, /probe, /config, /reload]
      │   → web_server_start() [port 80: config dashboard]
      └─ No creds? → AP mode → web_server_start() [port 80: WiFi config portal]
```

## JSON Config Format

The system uses `/spiffs/blackbox.json` for configuration:

```json
{
  "modules": [
    {
      "name": "http_2xx",
      "type": "MODULE_HTTP",
      "timeout_ms": 10000,
      "config": {
        "method": "GET",
        "valid_status_codes": [200],
        "valid_status_count": 1,
        "no_follow_redirects": false
      }
    }
  ],
  "targets": [
    {
      "name": "google_http",
      "module_name": "http_2xx",
      "target": "httpbin.org",
      "port": 80,
      "interval_ms": 30000
    }
  ],
  "scrape_interval": 30,
  "metrics_port": 9090
}
```

## HTTP Endpoints

### Port 9090 (metrics_server)
- GET /metrics - Prometheus format metrics for all targets
- GET /probe?target=X&module=Y - Single synchronous probe, returns Prometheus format
- GET /config - Current config JSON (modules + targets + global params)
- POST /reload - Hot reload configuration from SPIFFS

### Port 80 (web_server)
**AP Mode:**
- GET / - Dark theme WiFi config page
- GET /scan - WiFi scan results JSON
- POST /save - Save WiFi credentials to NVS

**STA Mode:**
- GET / - Config management dashboard
- GET /api/status - Device status JSON
- POST /api/config - Update configuration JSON
- POST /api/reload - Hot reload configuration

## Config Hot-Load

Configuration version tracking enables runtime updates:

```c
static uint8_t s_config_version = 0;

// Check for config updates each probe loop
void probe_manager_loop(void) {
    if (config_get_version() != s_config_version) {
        probe_manager_reload_targets();
        s_config_version = config_get_version();
    }
    // ... continue with probe execution ...
}
```

Changes to `/spiffs/blackbox.json` are automatically detected and applied without restarting.

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
