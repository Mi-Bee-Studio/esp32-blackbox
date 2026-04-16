#include "config_manager.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "CONFIG";

/* 配置管理器结构体，包含探测目标和指标端口 */
static blackbox_config_t s_config;
/* 静态探测目标配置 - 5个预定义的探测目标 */
static probe_target_t s_targets[] = {
    {
        .type = PROBE_TYPE_HTTP,
        .target = "httpbin.org",
        .port = 80,
        .interval_ms = 30000,
        .timeout_ms = 10000,
        .path = "/get",
        .verify_ssl = false,
    },
    {
        .type = PROBE_TYPE_HTTPS,
        .target = "httpbin.org",
        .port = 443,
        .interval_ms = 30000,
        .timeout_ms = 10000,
        .path = "/get",
        .verify_ssl = true,
    },
    {
        .type = PROBE_TYPE_TCP,
        .target = "8.8.8.8",
        .port = 53,
        .interval_ms = 30000,
        .timeout_ms = 5000,
        .path = "",
        .verify_ssl = false,
    },
    {
        .type = PROBE_TYPE_DNS,
        .target = "google.com",
        .port = 53,
        .interval_ms = 30000,
        .timeout_ms = 5000,
        .path = "",
        .verify_ssl = false,
    },
    {
        .type = PROBE_TYPE_TCP_TLS,
        .target = "google.com",
        .port = 443,
        .interval_ms = 30000,
        .timeout_ms = 10000,
        .path = "",
        .verify_ssl = false,
    },
};
esp_err_t config_manager_init(void)
{
    s_config.targets = s_targets;
    s_config.target_count = sizeof(s_targets) / sizeof(s_targets[0]);
    s_config.metrics_port = 9090;

    ESP_LOGI(TAG, "Config loaded: %d targets, metrics port: %d", 
             s_config.target_count, s_config.metrics_port);
    
    return ESP_OK;
}

const blackbox_config_t* config_manager_get_config(void)
{
    return &s_config;
}

/* ---- NVS WiFi credential helpers ---- */

bool config_manager_wifi_has_credentials(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t needed = 0;
    bool ok = (nvs_get_blob(nvs, NVS_KEY_SSID, NULL, &needed) == ESP_OK && needed > 0);
    nvs_close(nvs);
    return ok;
}

esp_err_t config_manager_wifi_save(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs, NVS_KEY_SSID, ssid, strlen(ssid) + 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save SSID failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    err = nvs_set_blob(nvs, NVS_KEY_PASS, password, strlen(password) + 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save PASS failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    err = nvs_commit(nvs);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }
    return err;
}

esp_err_t config_manager_wifi_load(char *ssid, size_t ssid_len,
                                   char *password, size_t pass_len)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    size_t len = ssid_len;
    err = nvs_get_blob(nvs, NVS_KEY_SSID, ssid, &len);
    if (err != ESP_OK) {
        nvs_close(nvs);
        return err;
    }

    len = pass_len;
    err = nvs_get_blob(nvs, NVS_KEY_PASS, password, &len);
    nvs_close(nvs);
    return err;
}

esp_err_t config_manager_wifi_clear(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_key(nvs, NVS_KEY_SSID);
    nvs_erase_key(nvs, NVS_KEY_PASS);
    err = nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "WiFi credentials cleared from NVS");
    return err;
}
