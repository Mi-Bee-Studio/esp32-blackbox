# AGENTS.md - main/

Application source. All modules follow the static module pattern with `s_` prefix globals and uppercase `TAG`. 现支持即席探测功能 `/probe`。
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
| `status_led.c` | LED 状态指示：板载 LED 显示设备状态 | ~401 | `status_led_init()`, `status_led_set_state()` |
| `status_led.h` | LED 状态 API 和枚举 | ~57 | `status_led_state_t` |
| `led_strip_encoder.c` | WS2812 RMT 编码器（仅 C6） | ~191 | `led_strip_encoder_new()` |
| `led_strip_encoder.h` | RMT 编码器头文件 | ~42 | `led_strip_encoder_t` |

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
  └─→ status_led ──→ driver/gpio (C3), driver/rmt_tx + led_strip_encoder (C6), esp_event
```

## Startup Flow

```
app_main()
  → nvs_flash_init()
  → esp_event_loop_create_default()
  → config_manager_init() [mounts SPIFFS, loads JSON config]
  → status_led_init() [LED 状态指示器初始化]
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
            "name": "httpbin_http",
      "module_name": "http_2xx",
      "target": "httpbin.org",
  ],
  "scrape_interval": 30,
  "metrics_port": 9090
}
```

## HTTP Endpoints

### Port 9090 (metrics_server)
- GET /metrics - Prometheus format metrics for all targets
- GET /probe?target=X&module=Y&port=P - Single synchronous probe with optional port, returns Prometheus format
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

- **即席探测支持**: `/probe` 端点支持动态目标配置，无需修改配置文件即可进行网络测试
- **mbedTLS v4**: No `entropy.h`, no `ctr_drbg.h`, no `mbedtls_ssl_conf_rng()`. TLS uses PSA Crypto internally — just configure `mbedtls_ssl_config` and call `mbedtls_ssl_setup()`.
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
| `ESP_STATUS_LED` | bool | `y` | 启用 LED 状态指示功能 |
| `ESP_LED_GPIO` | int | `8` (C3) / `15` (C6) | 状态 LED 的 GPIO 引脚号 |


## 即席探测使用教程

### 1. 基本用法

**HTTP 探测**:
```bash
#VYcurl "http://<设备IP>:9090/probe?target=httpbin.org&module=http_2xx"
```

**TCP 探测 (指定端口)**:
```bash
#WQcurl "http://<设备IP>:9090/probe?target=example.com&module=tcp&port=80"
```

**DNS 探测**:
```bash
#XXcurl "http://<设备IP>:9090/probe?target=8.8.8.8&module=dns"
```

### 2. 返回结果

**成功示例**:
```text
# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
#HNprobe_duration_seconds{target="httpbin.org", module="http_2xx"} 0.234

 HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
#HNprobe_success{target="httpbin.org", module="http_2xx"} 1
```

**失败示例**:
```text
# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
#HNprobe_duration_seconds{target="nonexistent.com", module="http_2xx"} 5.234

 HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
#HNprobe_success{target="nonexistent.com", module="http_2xx"} 0
```

### 3. 支持的模块类型

| 模块 | 类型 | 参数 | 说明 |
|------|------|------|------|
| `http_2xx` | HTTP | timeout_ms, method, valid_status_codes | HTTP/HTTPS 探测 |
| `tcp` | TCP | timeout_ms | TCP 连接测试 |
| `dns` | DNS | timeout_ms | DNS 解析测试 |
| `icmp_ping` | ICMP | timeout_ms, packets | ICMP Ping 测试 |
| `ws` | WebSocket | timeout_ms | WebSocket 连接测试 |
| `wss` | WebSocket Secure | timeout_ms | WebSocket + TLS 连接测试 |

### 4. 实际应用场景

**网络连通性测试**:
```bash
#VY# 测试 HTTP 服务可用性
curl "http://<设备IP>:9090/probe?target=your-service.com&module=http_2xx"

# 测试 TCP 端口连通性
#TMcurl "http://<设备IP>:9090/probe?target=your-server.com&module=tcp&port=443"
```

**多目标监控**:
```bash
#XX# 同时监控多个服务状态
curl "http://<设备IP>:9090/probe?target=api.example.com&module=http_2xx"

#STcurl "http://<设备IP>:9090/probe?target=database.example.com&module=tcp&port=5432"

#VYcurl "http://<设备IP>:9090/probe?target=cache.example.com&module=tcp&port=6379"
```

### 5. 集成到监控系统集成

**Prometheus Blackbox Exporter 配置**:
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

### 6. 即席探测的优势

✅ **无需配置文件修改**: 直接通过 URL 进行网络测试
✅ **快速验证**: 立即获得测试结果
✅ **灵活参数**: 支持任意主机名、IP 和端口
✅ **标准化输出**: Prometheus 格式，易于集成
✅ **调试友好**: 实时网络连通性检查

### 7. 使用注意事项

• 探测间隔默认为 5 秒，避免过频繁调用
• 建议在测试完成后间隔几秒再进行下一次探测
• TLS 探测会消耗较多资源，注意不要并发过多
• 超时时间默认为 10 秒，可根据网络环境调整
• 即席探测主要用于临时测试，长期监控建议使用配置文件
