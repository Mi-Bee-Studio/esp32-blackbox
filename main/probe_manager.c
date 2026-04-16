/*
 * probe_manager.c - 探测任务调度器实现
 *
 * 管理所有网络探测任务的调度和执行。
 * 在独立的 FreeRTOS 任务中循环执行，依次探测每个目标，
 * 每轮结束后等待 5 秒再开始下一轮。
 */

#include "probe_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config_manager.h"

static const char *TAG = "PROBE_MGR";

#define MAX_TARGETS 16

static probe_result_t s_results[MAX_TARGETS];
static TaskHandle_t s_probe_task_handle = NULL;

/**
 * @brief 探测任务主函数
 *
 * 无限循环执行所有配置的探测目标：
 * 1. 遍历每个目标，根据类型调用对应的探测函数
 * 2. 将结果存入 s_results 数组
 * 3. 每个探测之间间隔 100ms，避免看门狗超时
 * 4. 一轮完成后等待 5 秒再开始下一轮
 */
static void probe_task(void *pvParameters)
{
    const blackbox_config_t *config = config_manager_get_config();
    
    while (1) {
        for (int i = 0; i < config->target_count && i < MAX_TARGETS; i++) {
            const probe_target_t *target = &config->targets[i];
            probe_result_t result = {0};
            
            ESP_LOGI(TAG, "Probing target %d: %s:%d (type=%d)", 
                     i, target->target, target->port, target->type);
            
            switch (target->type) {
                case PROBE_TYPE_HTTP:
                    result = probe_http_execute(target);
                    break;
                case PROBE_TYPE_HTTPS:
                    result = probe_https_execute(target);
                    break;
                case PROBE_TYPE_TCP:
                    result = probe_tcp_execute(target);
                    break;
                case PROBE_TYPE_TCP_TLS:
                    result = probe_tcp_tls_execute(target);
                    break;
                case PROBE_TYPE_DNS:
                    result = probe_dns_execute(target);
                    break;
                case PROBE_TYPE_WS:
                    result = probe_ws_execute(target);
                    break;
                case PROBE_TYPE_WSS:
                    result = probe_wss_execute(target);
                    break;
                default:
                    strncpy(result.error_msg, "Unknown probe type", sizeof(result.error_msg));
                    break;
            }
            
            s_results[i] = result;
            
            ESP_LOGI(TAG, "Probe result: success=%d, duration=%dms, status=%d", 
                     result.success, result.duration_ms, result.status_code);
            
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t probe_manager_init(void)
{
    memset(s_results, 0, sizeof(s_results));
    ESP_LOGI(TAG, "Probe manager initialized");
    return ESP_OK;
}

esp_err_t probe_manager_start(void)
{
    BaseType_t ret = xTaskCreate(probe_task, "probe_task", 16384, NULL, 5, &s_probe_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create probe task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Probe manager started");
    return ESP_OK;
}

const probe_result_t* probe_manager_get_results(uint8_t *count)
{
    const blackbox_config_t *config = config_manager_get_config();
    if (count) {
        *count = config->target_count;
    }
    return s_results;
}

const probe_target_t* probe_manager_get_targets(uint8_t *count)
{
    const blackbox_config_t *config = config_manager_get_config();
    if (count) {
        *count = config->target_count;
    }
    return config->targets;
}
