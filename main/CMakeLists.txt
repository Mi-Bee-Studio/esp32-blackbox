# ESP32 Blackbox 组件 CMake 构建配置文件
# 注册所有源文件、头文件目录和依赖组件

# 注册 ESP-IDF 组件
idf_component_register(
    # 源文件列表 - 所有需要编译的 C 源文件
    SRCS 
        "main.c"                 # 应用入口点
        "wifi_manager.c"         # WiFi 连接管理
        "wifi_config_server.c"   # WiFi 配置服务器（Web 配网）
        "probe_http.c"           # HTTP/HTTPS 探测实现
        "probe_tcp.c"           # TCP/TCP+TLS 探测实现
        "probe_dns.c"           # DNS 探测实现
        "probe_ws.c"            # WebSocket/WSS 探测实现
        "metrics_server.c"      # Prometheus 指标服务器
        "config_manager.c"      # 配置管理
        "probe_manager.c"       # 探测任务调度器
    # 头文件搜索目录
    INCLUDE_DIRS "."
    # 依赖的 ESP-IDF 组件列表
    REQUIRES 
        nvs_flash              # 非易失性存储
        esp_wifi               # WiFi 驱动
        esp_http_client        # HTTP 客户端
        esp_http_server        # HTTP 服务器
        esp_https_ota          # HTTPS OTA 升级
        lwip                   # TCP/IP 协议栈
        esp_timer              # 定时器
        json                   # JSON 解析（本地 components/json）
        esp_event              # 事件循环
        log                    # 日志系统
        freertos              # 实时操作系统
        mbedtls               # TLS/SSL 加密库
)
