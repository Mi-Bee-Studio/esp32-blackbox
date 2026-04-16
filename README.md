# ESP32 Blackbox

基于 ESP32-C3 的网络探测设备，用于监控网络质量和端点可用性。支持首次启动 AP 配置模式，无需串口即可完成 WiFi 配置。

## 功能特性

- **零配置启动**: 首次上电自动进入 AP 模式，Web 页面配置 WiFi
- **多协议探测**: 支持 HTTP、HTTPS、TCP、TCP+TLS、DNS、WebSocket (WS/WSS)
- **Prometheus 指标**: 内置 Prometheus 格式的 metrics 服务器
- **WiFi 连接管理**: AP/STA 双模式，NVS 持久化 WiFi 凭据
- **灵活配置**: 通过 Kconfig 或 AP 配置页面配置参数

## 硬件要求

- ESP32-C3 开发板 (如 ESP32-C3-Mini)
- WiFi 网络连接
- USB 数据线 (烧录用)

## 软件依赖

- ESP-IDF **v6.0** (mbedTLS v4 + PSA Crypto)
- 本地组件: cJSON (`components/json/`)

## 快速开始

### 1. 首次配置 WiFi

给设备上电 → 连接 WiFi 热点 `ESP32_Blackbox` (密码: `12345678`) → 浏览器打开 `192.168.4.1` → 扫描 WiFi → 输入密码 → 保存重启

详细步骤见 [使用指南](docs/usage.md#首次配置-ap-模式)

### 2. 编译和烧录

```bash
idf.py set-target esp32c3
idf.py menuconfig       # 可选: 修改探测目标等配置
idf.py build
idf.py -p COM3 flash monitor
```

### 3. 修改探测目标

编辑 `main/config_manager.c` 中的 `s_targets[]` 数组:

```c
static probe_target_t s_targets[] = {
    {
        .type = PROBE_TYPE_HTTP,
        .target = "example.com",
        .port = 80,
        .interval_ms = 30000,
        .timeout_ms = 10000,
        .path = "/",
        .verify_ssl = false,
    },
    // 添加更多目标...
};
```

## 项目结构

```
esp32-blackbox/
├── main/
│   ├── main.c                  # 应用入口
│   ├── wifi_manager.c/h        # WiFi 连接管理 (AP/STA 双模式)
│   ├── wifi_config_server.c/h  # AP 模式 Web 配置服务器
│   ├── config_manager.c/h      # 配置管理 + NVS 存储
│   ├── probe_manager.c/h       # 探测任务调度
│   ├── probe_types.h           # 探测类型定义
│   ├── probe_http.c            # HTTP/HTTPS 探测
│   ├── probe_tcp.c             # TCP/TCP+TLS 探测
│   ├── probe_dns.c             # DNS 探测
│   ├── probe_ws.c              # WebSocket/WSS 探测
│   ├── metrics_server.c/h      # Prometheus metrics 服务器
│   ├── CMakeLists.txt          # 组件注册
│   └── Kconfig.projbuild       # Menuconfig 选项
├── components/
│   └── json/                   # 本地 cJSON 组件 (v6.0 兼容)
│       ├── cJSON.c
│       ├── cJSON.h
│       └── CMakeLists.txt
├── docs/                       # 文档目录
│   ├── architecture.md         # 架构设计
│   ├── design.md               # 设计文档
│   └── usage.md                # 使用指南
├── CMakeLists.txt              # 根项目配置
└── sdkconfig.defaults          # 默认配置
```

## 文档

更多文档请查看 [docs/](docs/) 目录:

- [架构设计](docs/architecture.md) — 模块划分、启动流程、线程模型
- [设计文档](docs/design.md) — 设计决策、性能考量、安全性
- [使用指南](docs/usage.md) — 环境准备、配置、烧录、故障排除

## Prometheus 集成

设备连接 WiFi 后访问 `http://<设备IP>:9090/metrics` 获取 Prometheus 指标:

```
# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
probe_duration_seconds{target="httpbin.org",port="80",type="http"} 0.234

# HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
probe_success{target="httpbin.org",port="80",type="http"} 1

# HELP probe_tls_handshake_seconds Duration of TLS handshake in seconds
# TYPE probe_tls_handshake_seconds gauge
probe_tls_handshake_seconds{target="google.com",port="443",type="tcp_tls"} 0.128
```

### Prometheus 配置示例

```yaml
scrape_configs:
  - job_name: 'esp32-blackbox'
    static_configs:
      - targets: ['192.168.1.100:9090']
```

## License

MIT
