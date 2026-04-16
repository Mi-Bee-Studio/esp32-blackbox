# ESP32 Blackbox 使用指南

## 目录

- [环境准备](#环境准备)
- [首次配置 (AP 模式)](#首次配置-ap-模式)
- [项目配置](#项目配置)
- [添加探测目标](#添加探测目标)
- [编译与烧录](#编译与烧录)
- [验证运行](#验证运行)
- [Prometheus 集成](#prometheus-集成)
- [故障排除](#故障排除)

## 环境准备

### 1. 安装 ESP-IDF v6.0

```bash
# 克隆 ESP-IDF v6.0
git clone --recursive -b v6.0 https://github.com/espressif/esp-idf.git

# 安装依赖
./install.sh

# 激活环境
. ./export.sh
```

要求 ESP-IDF **v6.0** (mbedTLS v4 + PSA Crypto API)。

### 2. 设置目标芯片

```bash
idf.py set-target esp32c3
```

### 3. 验证环境

```bash
idf.py --version
```

## 首次配置 (AP 模式)

设备首次启动时，NVS 中没有 WiFi 凭据，会自动进入 AP 配置模式。

### 步骤

1. **给设备上电**，等待约 5 秒启动完成
2. **打开手机/电脑 WiFi 设置**，搜索并连接以下热点:
   - SSID: `ESP32_Blackbox`
   - 密码: `12345678`
3. **打开浏览器**，访问 `http://192.168.4.1`
4. **点击「扫描 WiFi」** 按钮，等待附近 WiFi 列表加载
5. **选择你的 WiFi**，输入密码
6. **点击「保存并重启」**
7. 设备会自动保存凭据到 NVS 并重启
8. 重启后设备进入 STA 模式，自动连接你配置的 WiFi

### 配置页面功能

- WiFi 扫描: 列出附近的 WiFi 网络及信号强度
- 手动输入: 也可手动输入 SSID (适用于隐藏 WiFi)
- 信号指示: 显示各网络的信号强度
- 保存重启: 凭据写入 NVS，设备重启进入 STA 模式

### 重新配置

如需重新配置 WiFi，有两种方式:

**方式 1: 擦除 NVS (推荐)**
```bash
idf.py -p COM3 erase-flash
idf.py -p COM3 flash
```

**方式 2: 长按复位键**
- 按住 RST/EN 按钮不放，设备重启后可进入重新配置模式

## 项目配置

### WiFi 配置

WiFi 凭据可通过以下方式配置 (优先级从高到低):

1. **AP 配置页面** (运行时，存储到 NVS)
2. **Kconfig menuconfig** (编译时默认值)

运行 menuconfig:
```bash
idf.py menuconfig
```

导航至 `ESP32 Blackbox Configuration`:

```
ESP32 Blackbox Configuration
├── WiFi SSID           → 默认 WiFi 名称 (编译时)
├── WiFi Password       → 默认 WiFi 密码 (编译时)
├── AP SSID             → AP 热点名称 (默认 ESP32_Blackbox)
├── AP Password         → AP 热点密码 (默认 12345678)
└── Maximum retry       → 连接失败最大重试次数 (默认 5)
```

### Metrics 端口配置

编辑 `main/config_manager.c`:

```c
s_config.metrics_port = 9090;  // 修改为你需要的端口
```

## 添加探测目标

编辑 `main/config_manager.c` 中的 `s_targets[]` 数组。

### 支持的协议

| 协议 | type 值 | 说明 |
|------|---------|------|
| HTTP | `PROBE_TYPE_HTTP` | HTTP 明文连接 |
| HTTPS | `PROBE_TYPE_HTTPS` | HTTPS 加密连接 |
| TCP | `PROBE_TYPE_TCP` | TCP 连接测试 |
| TCP+TLS | `PROBE_TYPE_TCP_TLS` | TLS 加密 TCP 连接测试 |
| DNS | `PROBE_TYPE_DNS` | DNS 解析测试 |
| WS | `PROBE_TYPE_WS` | WebSocket |
| WSS | `PROBE_TYPE_WSS` | WebSocket Secure (TLS 加密) |

### 示例配置

#### HTTP 探测

```c
{
    .type = PROBE_TYPE_HTTP,
    .target = "example.com",
    .port = 80,
    .interval_ms = 30000,      // 30秒探测一次
    .timeout_ms = 10000,       // 10秒超时
    .path = "/",
    .verify_ssl = false,
},
```

#### HTTPS 探测

```c
{
    .type = PROBE_TYPE_HTTPS,
    .target = "example.com",
    .port = 443,
    .interval_ms = 30000,
    .timeout_ms = 10000,
    .path = "/api/health",
    .verify_ssl = true,        // 验证证书
},
```

#### TCP 连接探测

```c
{
    .type = PROBE_TYPE_TCP,
    .target = "8.8.8.8",       // 直接 IP 地址
    .port = 53,
    .interval_ms = 60000,      // 1分钟探测一次
    .timeout_ms = 5000,
    .path = "",
    .verify_ssl = false,
},
```

#### DNS 解析探测

```c
{
    .type = PROBE_TYPE_DNS,
    .target = "google.com",    // 域名
    .port = 53,
    .interval_ms = 60000,
    .timeout_ms = 5000,
    .path = "",
    .verify_ssl = false,
},
```

#### WebSocket 探测

```c
{
    .type = PROBE_TYPE_WS,
    .target = "echo.websocket.org",
    .port = 80,
    .interval_ms = 30000,
    .timeout_ms = 10000,
    .path = "/",
    .verify_ssl = false,
},
```

#### TCP+TLS 探测

测试目标端口的 TLS 握手是否成功，分别记录 TCP 连接耗时和 TLS 握手耗时:

```c
{
    .type = PROBE_TYPE_TCP_TLS,
    .target = "google.com",
    .port = 443,
    .interval_ms = 30000,
    .timeout_ms = 10000,
    .path = "",
    .verify_ssl = false,        // 设为 true 验证服务端证书
},
```

#### WebSocket Secure (WSS) 探测

通过 TLS 加密通道完成 WebSocket 握手，分别记录 TCP 连接、TLS 握手、WebSocket 握手耗时:

```c
{
    .type = PROBE_TYPE_WSS,
    .target = "echo.websocket.org",
    .port = 443,
    .interval_ms = 30000,
    .timeout_ms = 10000,
    .path = "/",
    .verify_ssl = false,
},
```

## 编译与烧录

### 1. 编译项目

```bash
idf.py build
```

### 2. 烧录固件

```bash
idf.py -p /dev/ttyUSB0 flash
```

Windows:
```bash
idf.py -p COM3 flash
```

### 3. 查看日志

```bash
idf.py monitor
```

按 `Ctrl+]` 退出监视器。

### 4. 烧录并监控

```bash
idf.py -p COM3 flash monitor
```

## 验证运行

### AP 模式 (首次启动)

正常启动日志:
```
I (xxx) MAIN: ESP32 Blackbox Probe Starting...
I (xxx) WIFI: NVS 中无 WiFi 凭据，启动 AP 模式
I (xxx) WIFI: AP SSID: ESP32_Blackbox, 密码: 12345678
I (xxx) WIFI: AP 已启动，等待用户配置...
I (xxx) CONFIG: 配置服务器已启动，监听端口 80
```

### STA 模式 (已配置 WiFi)

正常启动日志:
```
I (xxx) MAIN: ESP32 Blackbox Probe Starting...
I (xxx) WIFI: NVS 中找到 WiFi 凭据，启动 STA 模式
I (xxx) WIFI: 正在连接 SSID: YourWiFi
I (xxx) WIFI: wifi_init_sta finished.
I (xxx) WIFI: connected to ap SSID:YourWiFi
I (xxx) WIFI: got ip:192.168.1.100
I (xxx) MAIN: WiFi connected, starting services...
I (xxx) PROBE_MGR: Probe manager started
I (xxx) METRICS: Metrics server started on port 9090
I (xxx) MAIN: All services started
```

### 1. 获取设备 IP

在监视器日志中查看:
```
I (xxx) WIFI: got ip:192.168.1.100
```

### 2. 访问 Metrics 端点

```bash
curl http://192.168.1.100:9090/metrics
```

预期输出:
```
# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
probe_duration_seconds{target="example.com",port="80",type="http"} 0.234

# HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
probe_success{target="example.com",port="80",type="http"} 1

# HELP probe_status_code HTTP status code or TCP connect result
# TYPE probe_status_code gauge
probe_status_code{target="example.com",port="80",type="http"} 200

# HELP probe_tls_handshake_seconds Duration of TLS handshake in seconds
# TYPE probe_tls_handshake_seconds gauge
probe_tls_handshake_seconds{target="google.com",port="443",type="tcp_tls"} 0.128
```

### 3. 访问状态页面

```bash
curl http://192.168.1.100:9090/
```

## Prometheus 集成

### 1. 安装 Prometheus

参考: https://prometheus.io/download/

### 2. 配置 Prometheus

编辑 `prometheus.yml`:

```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'esp32-blackbox'
    static_configs:
      - targets:
          - '192.168.1.100:9090'  # 你的设备 IP
```

### 3. 启动 Prometheus

```bash
./prometheus --config.file=prometheus.yml
```

### 4. 访问 Prometheus UI

打开浏览器访问: http://localhost:9090

### 5. 查询指标

在 Expression 框中输入查询:

```
probe_success
probe_duration_seconds
probe_status_code
probe_tls_handshake_seconds
```

### Grafana 集成 (可选)

1. 安装 Grafana: https://grafana.com/docs/grafana/latest/setup/
2. 添加 Prometheus 数据源
3. 创建 Dashboard

示例查询:
```promql
# 探测成功率
avg(probe_success) by (job)

# 平均响应时间
avg(probe_duration_seconds) by (target)

# 按类型统计
count(probe_success == 1) by (type)
```

## 故障排除

### 搜索不到 AP 热点

1. 确认设备已上电并完成启动 (等待 5 秒)
2. 确认 AP 密码为 `12345678` (8位)
3. 靠近设备，WiFi 信号范围有限
4. 检查串口日志确认 AP 模式已启动
5. 尝试 `idf.py erase-flash` 擦除后重新烧录

### WiFi 连接失败

1. 检查 NVS 中的 SSID 和密码是否正确
2. 确认 WiFi 信号强度
3. 检查是否有特殊字符需要转义
4. 擦除 NVS 重新配置: `idf.py erase-flash`

### 探测全部失败

1. 确认目标服务器可达
2. 检查防火墙设置
3. 增加 timeout_ms 值

### Metrics 服务无响应

1. 确认设备已获取 IP
2. 检查 metrics_port 配置
3. 确认端口未被占用

### 编译错误

1. 确保 ESP-IDF v6.0 安装完整
2. 目标已设置: `idf.py set-target esp32c3`
3. 更新子模块: `git submodule update --init --recursive`
4. 清理并重新编译: `idf.py fullclean && idf.py build`
5. 确认 `components/json/` 目录存在 (v6.0 本地 cJSON 组件)

### TLS 握手失败

1. 确认目标端口支持 TLS (通常为 443)
2. 如需证书验证, 确保设备时间同步 (NTP)
3. 尝试设置 `verify_ssl = false` 排除证书问题
4. 检查日志中的 mbedTLS 错误码 (格式 `-0xXXXX`), 常见错误:
   - `-0x2700`: 证书验证失败
   - `-0x7200`: SSL 握手超时
   - `-0x0050`: 网络发送失败
