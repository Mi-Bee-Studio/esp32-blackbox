# ESP32 Blackbox

基于 ESP32 的网络探测设备，兼容 Prometheus blackbox_exporter 架构，支持 JSON 配置和 Web UI 管理。

## 功能特性

- **零配置启动**: 首次上电自动进入 AP 模式，Web 页面配置 WiFi
- **配置驱动探测**: JSON + SPIFFS，无需重编译即可修改探测目标
- **blackbox_exporter 兼容**: /probe 端点，支持 Prometheus 主动抓取
- **ICMP Ping 探测**: 原生 socket 实现
- **配置热加载**: 运行时修改配置无需重启
- **Web UI 配置管理**: AP 模式 WiFi 配置 + STA 模式探测管理
- **多协议探测**: HTTP/HTTPS/TCP/TLS/DNS/ICMP/WebSocket (WS/WSS)
- **双目标支持**: ESP32-C3 和 ESP32-C6 (WiFi 6)

## 硬件要求

- ESP32-C3 开发板 (如 ESP32-C3 SuperMini)
- ESP32-C6 开发板 (如 Seeed Studio XIAO ESP32C6)
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

**方式一：Python 构建脚本（推荐）**

```bash
# ESP32-C6 构建
python build.py esp32c6 build

# ESP32-C6 构建并烧录
python build.py esp32c6 flash COM3

# ESP32-C3 构建并烧录+监控
python build.py esp32c3 monitor COM3
```

**方式二：ESP-IDF 命令行（需先运行 export.bat）**

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

**方式三：Windows 批处理脚本**

```cmd
build_target.bat esp32c6 build          # 仅构建
build_target.bat esp32c6 flash COM3     # 构建并烧录
build_target.bat esp32c6 clean          # 全量清理后构建
build_target.bat esp32c3 monitor COM3   # 构建烧录监控
```

### 3. 修改探测目标

支持两种配置方式：

**方法一：Web UI 配置**

浏览器打开 `http://<设备IP>/` → 编辑 JSON 配置 → 保存

**方法二：JSON 文件配置**

编辑 `/spiffs/blackbox.json` 配置文件（可通过 Web UI 或 API 访问）

配置文件格式：

```json
{
  "scrape_interval": 30,
  "metrics_port": 9090,
  "modules": {
    "http_2xx": {
      "prober": "http",
      "timeout": 10,
      "http": {
        "method": "GET",
        "valid_status_codes": [200]
      }
    },
    "icmp_ping": {
      "prober": "icmp",
      "timeout": 5,
      "icmp": {
        "packets": 3,
        "payload_size": 56
      }
    }
  },
  "targets": [
    {
      "name": "google_http",
      "target": "httpbin.org",
      "port": 80,
      "interval": 30,
      "module": "http_2xx"
    },
    {
      "name": "google_dns",
      "target": "8.8.8.8",
      "port": 53,
      "interval": 30,
      "module": "icmp_ping"
    }
  ]
}
```

## 项目结构

```
esp32-blackbox/
├── main/
│   ├── main.c                  # 应用入口
│   ├── wifi_manager.c/h        # WiFi 连接管理 (AP/STA 双模式)
│   ├── web_server.c/h          # 统一 Web 服务器 (AP 配置 + STA 管理)
│   ├── web_server.c/h          # STA 模式配置管理 Web 服务器
│   ├── config_manager.c/h      # 配置管理 + NVS 存储 + JSON SPIFFS
│   ├── probe_manager.c/h       # 探测任务调度
│   ├── probe_types.h           # 探测类型定义
│   ├── probe_http.c            # HTTP/HTTPS 探测
│   ├── probe_tcp.c             # TCP/TCP+TLS 探测
│   ├── probe_dns.c             # DNS 探测
│   ├── probe_icmp.c            # ICMP Ping 探测
│   ├── probe_ws.c              # WebSocket/WSS 探测
│   ├── metrics_server.c/h      # Prometheus metrics + /probe 端点
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
├── sdkconfig.defaults          # 默认配置
├── sdkconfig.defaults.esp32c3  # ESP32-C3 配置
├── sdkconfig.defaults.esp32c6  # ESP32-C6 配置
├── partitions.csv              # 自定义分区表 (含 SPIFFS)
├── build.py                    # Python 构建脚本 (推荐)
└── build_target.bat            # Windows 批处理构建脚本
```

## 文档

更多文档请查看 [docs/](docs/) 目录:

- [架构设计](docs/architecture.md) — 模块划分、启动流程、线程模型
- [设计文档](docs/design.md) — 设计决策、性能考量、安全性
- [使用指南](docs/usage.md) — 环境准备、配置、烧录、故障排除

## Prometheus 集成

设备连接 WiFi 后访问 `http://<设备IP>:9090/metrics` 获取 Prometheus 指标:

```
# HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
probe_success{target="google_http",module="http_2xx"} 1

# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
probe_duration_seconds{target="google_http",module="http_2xx"} 0.234

# HELP probe_http_status_code HTTP status code
# TYPE probe_http_status_code gauge
probe_http_status_code{target="google_http",module="http_2xx"} 200
```

### Prometheus blackbox_exporter Scrape 配置

```yaml
scrape_configs:
  - job_name: 'blackbox_http'
    metrics_path: /probe
    params:
      module: [http_2xx]
    static_configs:
      - targets: ['httpbin.org:80']
    relabel_configs:
      - source_labels: [__address__]
        target_label: __param_target
      - target_label: __address__
        replacement: 192.168.1.100:9090

  - job_name: 'esp32-blackbox'
    static_configs:
      - targets: ['192.168.1.100:9090']
```

## HTTP 端点

### 端口 9090 (metrics_server)
- `GET /metrics` — 所有 target 的 Prometheus 格式指标
- `GET /probe?target=X&module=Y` — 单次同步探测，返回 Prometheus 格式
- `GET /config` — 当前 JSON 配置
- `POST /reload` — 热加载 SPIFFS 配置

### 端口 80 (web_server)
**AP 模式:**
- `GET /` — WiFi 配置页面
- `GET /scan` — WiFi 扫描结果 JSON
- `POST /save` — 保存 WiFi 凭据到 NVS

**STA 模式:**
- `GET /` — 配置管理仪表板
- `GET /api/status` — 设备状态 JSON
- `POST /api/config` — 更新配置 JSON
- `POST /api/reload` — 热加载配置

## License

MIT
