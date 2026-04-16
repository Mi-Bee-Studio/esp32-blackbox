/*
 * main.c - ESP32 Blackbox 主程序入口
 *
 * 本文件是 ESP32 Blackbox 应用程序的主入口点，负责初始化系统并启动所有核心服务。
 * 主要职责包括：
 * 1. 初始化非易失性存储 (NVS) 用于保存 WiFi 密码
 * 2. 创建默认事件循环
 * 3. 初始化配置管理器 (加载探测目标) 和 WiFi 管理器
 * 4. 处理 WiFi 密码存储逻辑，决定使用 STA 或 AP 模式
 * 5. 启动配置门户或正常服务
 *
 * 系统流程：
 * - 初始化 NVS
 * - 检查是否存储了 WiFi 密码
 *   - 是：尝试 STA 模式连接
 *     - 连接成功：启动探测管理和指标服务器
 *     - 连接失败：切换到 AP 配置门户
 *   - 否：直接启动 AP 配置门户
 * - 在 AP 模式下，用户连接后可配置 WiFi 密码
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "wifi_config_server.h"
#include "config_manager.h"
#include "probe_manager.h"
#include "metrics_server.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    /* 初始化 NVS (非易失性存储) 用于保存 WiFi 密码 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 创建默认事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "ESP32 Blackbox Starting...");

    /* 初始化配置管理器和 WiFi 管理器 */
    config_manager_init();
    wifi_manager_init();

    /* 检查是否已存储 WiFi 密码 */
    if (config_manager_wifi_has_credentials()) {
        /* 存储了密码 - 尝试 STA 模式连接 */
        ESP_LOGI(TAG, "WiFi credentials found in NVS, connecting...");

        ret = wifi_manager_start_sta();
        if (ret == ESP_OK) {
            /* 连接成功 - 启动探测管理和指标服务器 */
            probe_manager_init();
            probe_manager_start();
            metrics_server_start();
            ESP_LOGI(TAG, "All services started");
            return;
        }

        /* STA 连接失败 - 切换到 AP 配置门户 */
        ESP_LOGW(TAG, "STA failed, falling back to AP config portal");
        wifi_manager_stop();
    } else {
        ESP_LOGI(TAG, "No WiFi credentials - starting AP config portal");
    }

    /* 启动 AP 模式配置门户 */
    wifi_manager_start_ap();
    wifi_config_server_start();

    ESP_LOGI(TAG, "Config portal active - connect to AP '%s' and open 192.168.4.1",
             CONFIG_ESP_AP_SSID);
}
