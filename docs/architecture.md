# ESP32 Blackbox 架构设计

## 系统概述

ESP32 Blackbox 是一个基于 ESP32-C3 的嵌入式网络探测系统，采用模块化设计，支持首次启动 AP 配置模式。主要分为以下层次:

```
┌─────────────────────────────────────────────────────┐
│                Application Layer                    │
│  ┌─────────────┐  ┌───────────────────────────┐    │
│  │    main.c    │  │    Metrics HTTP Server    │    │
│  └─────────────┘  └───────────────────────────┘    │
├─────────────────────────────────────────────────────┤
│                Service Layer                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐  │
│  │ WiFi Manager │  │ Probe Manager│  │  WiFi    │  │
│  │ (AP/STA)     │  │              │  │  Config  │  │
│  └──────────────┘  └──────────────┘  │  Server  │  │
│                                     └──────────┘  │
├─────────────────────────────────────────────────────┤
│                Probe Layer                          │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐      │
│  │  HTTP  │ │TCP/TLS │ │  DNS   │ │ WS/WSS │      │
│  └────────┘ └────────┘ └────────┘ └────────┘      │
├─────────────────────────────────────────────────────┤
│                Config / Storage Layer               │
│         Config Manager  +  NVS Flash                │
└─────────────────────────────────────────────────────┘
```

## 核心模块

### 1. WiFi Manager (`wifi_manager.c/h`)

负责 WiFi 连接管理，支持 AP 和 STA 双模式。

**功能:**
- 初始化 WiFi，自动检测 NVS 中是否已存储 WiFi 凭据
- AP 模式: 启动热点供用户配置 WiFi (SSID: `ESP32_Blackbox`, 密码: `12345678`)
- STA 模式: 连接已配置的 WiFi 网络
- 事件驱动连接管理，自动重连机制
- 连接状态查询

**关键设计:**
- 使用 FreeRTOS EventGroup 同步连接状态
- 首次启动无凭据时自动进入 AP 模式
- NVS 存储 WiFi SSID 和密码，重启后自动连接
- 支持配置最大重试次数 (默认 5 次)

### 2. WiFi Config Server (`wifi_config_server.c/h`)

AP 模式下的 Web 配置服务器。

**功能:**
- 在 AP 模式下启动 HTTP 服务器 (端口 80)
- 提供暗色主题配置页面 (`/`)
- 支持 WiFi 扫描并列出可用网络
- 接收用户提交的 SSID 和密码
- 将凭据保存至 NVS 并重启设备进入 STA 模式

**工作流程:**
```
用户连接 AP → 访问 192.168.4.1 → 扫描 WiFi → 输入密码 → 保存 → 重启
```

### 3. Config Manager (`config_manager.c/h`)

统一管理系统配置。

**数据结构:**
```c
typedef struct {
    probe_target_t *targets;     // 探测目标数组
    uint8_t target_count;        // 目标数量
    uint16_t metrics_port;       // Metrics 端口
} blackbox_config_t;
```

**探测目标配置:**
```c
typedef struct {
    probe_type_t type;           // 探测类型
    char target[256];            // 目标地址
    uint16_t port;               // 端口
    uint32_t interval_ms;        // 探测间隔
    uint32_t timeout_ms;         // 超时时间
    char path[128];              // HTTP/WebSocket 路径
    bool verify_ssl;             // 是否验证 SSL 证书
} probe_target_t;
```

### 4. Probe Manager (`probe_manager.c/h`)

探测任务调度器。

**职责:**
- 管理探测任务的生命周期
- 轮询执行所有配置的探测
- 存储探测结果供 metrics 服务器使用

**任务流程:**
```
while (1) {
    for (每个探测目标) {
        执行对应协议的探测
        保存结果
    }
    等待 interval_ms
}
```

### 5. Probe Modules

各协议的探测实现。

| 模块 | 文件 | 支持类型 |
|------|------|----------|
| HTTP | `probe_http.c` | HTTP, HTTPS |
| TCP | `probe_tcp.c` | TCP, TCP+TLS |
| DNS | `probe_dns.c` | DNS |
| WebSocket | `probe_ws.c` | WS, WSS |

**探测结果结构:**
```c
typedef struct {
    bool success;               // 是否成功
    uint32_t duration_ms;       // 总耗时
    int status_code;            // 状态码
    char error_msg[128];        // 错误信息
    union {
        struct { /* HTTP 详情: connect_time, tls_time, ttfb, status */ } http;
        struct { /* TCP 详情: connect_time, tls_time */ } tcp;
        struct { /* DNS 详情: resolve_time, resolved_ip */ } dns;
        struct { /* WS 详情: connect_time, tls_time, handshake_time */ } ws;
    } details;
} probe_result_t;
```

TCP+TLS 和 WSS 探测通过 mbedTLS v4 库实现 TLS 握手，使用 PSA Crypto API，并分别记录 TCP 连接耗时和 TLS 握手耗时。

### 6. Metrics Server (`metrics_server.c/h`)

Prometheus 指标导出服务器。

**功能:**
- HTTP 服务器，监听配置端口
- 提供 `/metrics` 端点输出 Prometheus 格式指标
- 提供根路径 `/` 显示简单状态页面

**输出指标:**
- `probe_duration_seconds`: 探测耗时 (秒)
- `probe_success`: 探测是否成功 (0/1)
- `probe_status_code`: HTTP 状态码或 TCP 连接结果
- `probe_tls_handshake_seconds`: TLS 握手耗时 (秒), 仅 TCP_TLS 和 WSS 类型

## 启动流程

```
app_main()
  │
  ├─ nvs_flash_init()              // 初始化 NVS
  │
  ├─ esp_event_loop_create_default()
  │
  ├─ config_manager_init()         // 加载配置
  │
  ├─ wifi_manager_init()
  │   ├─ 检查 NVS 中是否有 WiFi 凭据
  │   │   ├─ 无凭据 → 启动 AP 模式
  │   │   │           ├─ wifi_config_server_start()  // 启动配置 Web 服务器
  │   │   │           └─ 等待用户配置
  │   │   └─ 有凭据 → 启动 STA 模式
  │   │               └─ 等待 WiFi 连接
  │   └─ wifi_manager_start()
  │
  ├─ probe_manager_init()
  │   └─ probe_manager_start()     // 启动探测任务
  │
  └─ metrics_server_start()        // 启动 HTTP 服务器
```

## 线程模型

系统使用 FreeRTOS 多任务架构:

| 任务名 | 优先级 | 栈大小 | 功能 |
|--------|--------|--------|------|
| probe_task | 5 | 16KB | 执行网络探测 (含 TLS) |
| metrics_server | 5 | 8KB | Prometheus HTTP 服务器 |
| wifi_config_server | 5 | 4KB | AP 模式配置 Web 服务器 |
| WiFi events | 系统 | - | WiFi 事件处理 |

## 配置机制

### Kconfig 配置
- WiFi SSID/密码 (默认值，可被 AP 配置覆盖)
- AP 模式 SSID/密码
- 最大重试次数

### NVS 存储
- WiFi SSID 和密码 (用户通过 AP 配置页面保存)
- WiFi 配置标志位 (标识是否已完成首次配置)

### 代码配置
- 探测目标列表 (config_manager.c)
- Metrics 端口

## 数据流

```
[NVS WiFi 凭据] ─→ WiFi Manager ─→ STA 连接
                                    │
                                    ├─→ Probe Manager ─→ 各协议探测
                                    │                     │
                                    │                     └─→ 结果存储
                                    │                           │
                                    └─→ Metrics Server ←────────┘
                                              │
                                              └─→ Prometheus 抓取
```

## 扩展计划

- [x] WSS (WebSocket Secure) 探测实现
- [x] TCP+TLS 加密连接探测实现
- [x] NVS 存储 WiFi 凭据持久化
- [x] AP 模式 Web 配置页面
- [x] ESP-IDF v6.0 迁移 (mbedTLS v4 / PSA Crypto)
- [ ] mDNS 服务发现
- [ ] OTA 固件更新
- [ ] 告警机制
- [ ] 探测目标动态配置 (Web UI)
