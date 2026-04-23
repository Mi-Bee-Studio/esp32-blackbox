# ESP32 Blackbox

[English](README.md) | **中文**

## 概述

ESP32 Blackbox 是一个网络探测设备，兼容 Prometheus Blackbox Exporter，可以通过 HTTP API 进行网络连通性测试。

**主要特性**：
- 支持 HTTP/HTTPS, TCP, DNS, ICMP, WebSocket, WebSocket Secure 等多种探测类型
- 内置 WiFi 配置门户，支持动态连接配置
- 兼容 Prometheus Blackbox Exporter，可通过 Prometheus 监控
- 即席探测功能 `/probe`，无需配置文件即可进行网络测试
- 硬件自检功能，自动验证板卡功能
- 支持多种 ESP32 板卡（ESP32-C3 SuperMini, Seeed Studio XIAO ESP32C6）

## 技术栈

- **开发环境**: ESP-IDF v6.0
- **编程语言**: C (gnu23)
- **操作系统**: FreeRTOS
- **网络栈**: LWIP
- **加密库**: mbedTLS v4 (PSA Crypto)
- **目标板卡**: ESP32-C3 SuperMini | Seeed Studio XIAO ESP32C6

## 快速开始

### 方式一：Python 构建脚本（推荐，脱离 ESP-IDF 终端）

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

### 方式二：ESP-IDF 命令行（需先运行 export.bat）

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

### 方式三：Windows 批处理脚本

```cmd
build_target.bat esp32c6 build          # 仅构建
build_target.bat esp32c6 flash COM3     # 构建并烧录
build_target.bat esp32c6 clean          # 全量清理后构建
build_target.bat esp32c3 monitor COM3   # 构建烧录监控
```

## 板卡支持

| 板卡 | 芯片 | 架构 | SRAM | Flash | WiFi | 特殊功能 |
|------|------|------|------|-------|------|----------|
| ESP32-C3 SuperMini (无标版) | ESP32-C3 | RISC-V 单核 160MHz | 400KB | 4MB (嵌入) | 802.11 b/g/n | - |
| Seeed Studio XIAO ESP32C6 | ESP32-C6 | RISC-V 双核 (HP 160MHz + LP 20MHz) | 512KB | 4MB (外挂) | 802.11ax (WiFi 6) | BLE 5.3, Zigbee, Thread |

## 板卡自检（硬件自检）

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

## 配置方法

### 1. WiFi 配置

**通过 AP 配置门户**：
- SSID: `ESP32_Blackbox`
- 密码: `12345678`
- IP: `192.168.4.1`

**通过 Web 界面**：
访问 `http://192.168.4.1` 进行 WiFi 配置

### 2. 探测配置

探测配置文件位于 `/spiffs/blackbox.json`，格式如下：

```json
{
  "metrics": {
    "port": 9090
  },
  "modules": [
    {
      "name": "http_2xx",
      "type": "http_2xx",
      "config": {
        "timeout_ms": 5000,
        "method": "GET",
        "expected_status": [200],
        "tls": false
      }
    },
    {
      "name": "tcp",
      "type": "tcp",
      "config": {
        "timeout_ms": 5000
      }
    }
  ],
  "targets": [
    {
      "name": "google_dns",
      "module_name": "dns",
      "target": "8.8.8.8",
      "interval_ms": 5000
    },
    {
      "name": "httpbin_test",
      "module_name": "http_2xx", 
      "target": "httpbin.org",
      "port": 80,
      "interval_ms": 30000
    }
  ]
}
```

### 3. 即席探测

支持通过 `/probe` 端点进行即席探测：

```bash
# HTTP 探测
curl "http://<设备IP>:9090/probe?target=httpbin.org&module=http_2xx"

# TCP 探测
curl "http://<设备IP>:9090/probe?target=example.com&module=tcp&port=443"

# DNS 探测
curl "http://<设备IP>:9090/probe?target=8.8.8.8&module=dns"
```

## 目录结构

```
esp32-blackbox/
├── main/                    # 应用程序源码
├── components/json/         # 依赖的 cJSON 库
├── docs/                   # 文档目录
│   ├── zh/                # 中文文档
│   │   ├── architecture.md    # 架构设计
│   │   ├── design.md         # 详细设计  
│   │   └── usage.md          # 使用教程
│   └── en/                # 英文文档
│       ├── architecture.md    # Architecture
│       ├── design.md         # Design
│       └── usage.md          # Usage
├── CMakeLists.txt          # 根项目配置
├── sdkconfig.defaults       # 通用默认配置
├── sdkconfig.defaults.esp32c3  # ESP32-C3 特定配置
├── sdkconfig.defaults.esp32c6  # ESP32-C6 特定配置
├── partitions.csv           # 自定义分区表
├── build.py                # Python 构建脚本
├── build_target.bat        # Windows 批处理构建脚本
├── README.md               # 英文说明文档
├── README_CN.md            # 中文说明文档
└── AGENTS.md               # 项目说明
```

## 开发指南

### 代码规范

- **4 空格缩进**，大括号同行，120 字符最大宽度
- **注释全部使用中文**，代码注释双语（中文描述）
- **静态全局变量**：`s_` 前缀（例如 `s_state`, `s_probe_task_handle`）
- **Log TAGs**：大写模块缩写（例如 `TAG = "WIFI"`, `TAG = "PROBE_MGR"`）

### 添加新探测类型

1. 在 `config_manager.h` 中添加探测模块类型枚举
2. 在 `config_manager.h` 中添加模块配置结构联合体
3. 创建 `probe_xxx.c` 文件并实现执行函数
4. 在 `probe_types.h` 中添加函数签名
5. 在 `config_manager.c` 的 `s_factory_modules[]` 中添加默认配置
6. 在 `probe_manager.c` 的 switch 语句中注册
7. 更新 `main/CMakeLists.txt` 的 SRCS + 更新 `metrics_server.c` 的类型字符串映射

### 添加新板卡支持

1. 创建 `sdkconfig.defaults.<target>` 文件配置 flash 和加密设置
2. 在 `main.c` 的 `print_board_info()` 中添加 `#elif CONFIG_IDF_TARGET_<TARGET>`
3. 在 `Kconfig.projbuild` 中为 `ESP_BOARD_TEST` 添加目标特定的默认值
4. 使用 `python build.py <target> clean` 测试

## 监控与集成

### Prometheus 配置

```yaml
scrape_configs:
  - job_name: 'esp32_blackbox'
    static_configs:
      - targets:
          - '192.168.1.100:9090'  # 替换为设备 IP
```

### 即席探测集成

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

## 故障排除

### 常见问题

1. **WiFi 连接失败**
   - 检查 SSID 和密码是否正确
   - 确认路由器支持 2.4GHz 频段
   - 重启设备并重试

2. **构建失败**
   - 确认 ESP-IDF 环境正确配置
   - 检查目标芯片设置是否正确
   - 使用 `idf.py fullclean` 清理后重新构建

3. **设备无响应**
   - 检查串口连接和波特率
   - 确认电源供电稳定
   - 检查板卡是否正确识别

### 调试模式

启用调试日志：
```bash
idf.py menuconfig
# 调试日志 → 日志级别 → Info
```

或通过串口监控：
```bash
idf.py -p COM3 flash monitor
```

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。