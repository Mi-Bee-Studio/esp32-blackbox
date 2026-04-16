#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* 探测类型枚举 */
typedef enum {
    PROBE_TYPE_HTTP,        /* HTTP 探测 */
    PROBE_TYPE_HTTPS,       /* HTTPS 探测 */
    PROBE_TYPE_TCP,         /* TCP 探测 */
    PROBE_TYPE_TCP_TLS,     /* TCP+TLS 探测 */
    PROBE_TYPE_DNS,         /* DNS 探测 */
    PROBE_TYPE_WS,          /* WebSocket 探测 */
    PROBE_TYPE_WSS,         /* WSS 探测 */
} probe_type_t;

/* 探测目标结构体 */
typedef struct {
    probe_type_t type;     /* 探测类型 */
    char target[256];       /* 目标主机名 */
    uint16_t port;         /* 目标端口 */
    uint32_t interval_ms;  /* 探测间隔 (毫秒) */
    uint32_t timeout_ms;   /* 超时时间 (毫秒) */
    char path[128];        /* 探测路径 */
    bool verify_ssl;       /* 是否验证 SSL 证书 */
} probe_target_t;

/* 黑盒配置结构体 */
typedef struct {
    probe_target_t *targets;   /* 探测目标数组 */
    uint8_t target_count;      /* 探测目标数量 */
    uint16_t metrics_port;     /* 指标服务器端口 */
} blackbox_config_t;

/* 初始化配置管理器 */
esp_err_t config_manager_init(void);

/* 获取配置信息 */
const blackbox_config_t* config_manager_get_config(void);

/* NVS WiFi 凭证存储 */
#define NVS_NAMESPACE  "blackbox"   /* NVS 命名空间 */
#define NVS_KEY_SSID   "wifi_ssid"  /* WiFi SSID 键 */
#define NVS_KEY_PASS   "wifi_pass"  /* WiFi 密码键 */

/* 检查是否已存储 WiFi 凭证 */
bool config_manager_wifi_has_credentials(void);

/* 保存 WiFi 凭证到 NVS */
esp_err_t config_manager_wifi_save(const char *ssid, const char *password);

/* 从 NVS 加载 WiFi 凭证 */
esp_err_t config_manager_wifi_load(char *ssid, size_t ssid_len,
                                   char *password, size_t pass_len);

/* 清除 NVS 中的 WiFi 凭证 */
esp_err_t config_manager_wifi_clear(void);

#endif
