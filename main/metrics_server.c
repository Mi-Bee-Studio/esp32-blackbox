/* 
 * Prometheus 指标 HTTP 服务器实现
 * 
 * 该模块实现 Prometheus 格式的指标服务器，用于提供系统监控数据。
 * 支持通过 HTTP 请求获取指标数据，包括探测时间、成功状态、状态码等。
 * 
 * 功能：
 * 1. 生成 Prometheus 指标文本格式输出
 * 2. 处理 HTTP 请求：/metrics 返回指标数据，/ 返回 HTML 页面
 * 3. 创建 FreeRTOS 任务运行 HTTP 服务器
 */

#include "metrics_server.h"
#include <string.h>
#include <stdio.h>
#include "lwip/sockets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "probe_manager.h"
#include "config_manager.h"

static const char *TAG = "METRICS";

#define METRICS_BUFFER_SIZE 4096

static const char *get_probe_type_str(probe_type_t type)
{
    switch (type) {
        case PROBE_TYPE_HTTP: return "http";
        case PROBE_TYPE_HTTPS: return "https";
        case PROBE_TYPE_TCP: return "tcp";
        case PROBE_TYPE_TCP_TLS: return "tcp_tls";
        case PROBE_TYPE_DNS: return "dns";
        case PROBE_TYPE_WS: return "ws";
        case PROBE_TYPE_WSS: return "wss";
        default: return "unknown";
    }
}

static int generate_metrics(char *buffer, size_t buffer_size)
{
    int len = 0;
    
    len += snprintf(buffer + len, buffer_size - len,
        "# HELP probe_duration_seconds Duration of the probe in seconds\n"
        "# TYPE probe_duration_seconds gauge\n"
        "# HELP probe_success Whether the probe succeeded\n"
        "# TYPE probe_success gauge\n"
        "# HELP probe_status_code HTTP status code or TCP connect result\n"
        "# TYPE probe_status_code gauge\n"
        "# HELP probe_tls_handshake_seconds Duration of TLS handshake in seconds\n"
        "# TYPE probe_tls_handshake_seconds gauge\n"
    );
    
    uint8_t count;
    const probe_target_t *targets = probe_manager_get_targets(&count);
    const probe_result_t *results = probe_manager_get_results(&count);
    
    for (int i = 0; i < count; i++) {
        const probe_target_t *target = &targets[i];
        const probe_result_t *result = &results[i];
        
        float duration_sec = result->duration_ms / 1000.0f;
        int success = result->success ? 1 : 0;
        
        len += snprintf(buffer + len, buffer_size - len,
            "probe_duration_seconds{target=\"%s\",port=\"%d\",type=\"%s\"} %.3f\n",
            target->target, target->port, get_probe_type_str(target->type), duration_sec
        );
        
        len += snprintf(buffer + len, buffer_size - len,
            "probe_success{target=\"%s\",port=\"%d\",type=\"%s\"} %d\n",
            target->target, target->port, get_probe_type_str(target->type), success
        );
        
        len += snprintf(buffer + len, buffer_size - len,
            "probe_status_code{target=\"%s\",port=\"%d\",type=\"%s\"} %d\n",
            target->target, target->port, get_probe_type_str(target->type), result->status_code
        );
        
        if (target->type == PROBE_TYPE_TCP_TLS) {
            float tls_sec = result->details.tcp.tls_time_ms / 1000.0f;
            len += snprintf(buffer + len, buffer_size - len,
                "probe_tls_handshake_seconds{target=\"%s\",port=\"%d\",type=\"%s\"} %.3f\n",
                target->target, target->port, get_probe_type_str(target->type), tls_sec
            );
        } else if (target->type == PROBE_TYPE_WSS) {
            float tls_sec = result->details.ws.tls_time_ms / 1000.0f;
            len += snprintf(buffer + len, buffer_size - len,
                "probe_tls_handshake_seconds{target=\"%s\",port=\"%d\",type=\"%s\"} %.3f\n",
                target->target, target->port, get_probe_type_str(target->type), tls_sec
            );
        }
    }
    
    return len;
}

static void http_server_task(void *pvParameters)
{
    const blackbox_config_t *config = config_manager_get_config();
    int port = config->metrics_port;
    
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }
    
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    if (listen(listen_sock, 5) != 0) {
        ESP_LOGE(TAG, "Failed to listen on socket");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Metrics server started on port %d", port);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Failed to accept connection");
            continue;
        }
        
        char recv_buffer[512];
        int recv_len = recv(client_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
        
        if (recv_len > 0) {
            recv_buffer[recv_len] = '\0';
            
            if (strstr(recv_buffer, "GET /metrics") != NULL) {
                char metrics_buffer[METRICS_BUFFER_SIZE];
                int metrics_len = generate_metrics(metrics_buffer, sizeof(metrics_buffer));
                
                char response[METRICS_BUFFER_SIZE + 256];
                int response_len = snprintf(response, sizeof(response),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain; charset=utf-8\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    metrics_len, metrics_buffer
                );
                
                send(client_sock, response, response_len, 0);
            } else if (strstr(recv_buffer, "GET /") != NULL) {
                const char *response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "<h1>ESP32 Blackbox Exporter</h1>"
                    "<p>Visit <a href=\"/metrics\">/metrics</a> for Prometheus metrics</p>";
                send(client_sock, response, strlen(response), 0);
            } else {
                const char *response = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Not Found";
                send(client_sock, response, strlen(response), 0);
            }
        }
        
        close(client_sock);
    }
    
    close(listen_sock);
    vTaskDelete(NULL);
}

esp_err_t metrics_server_start(void)
{
    BaseType_t ret = xTaskCreate(http_server_task, "metrics_server", 8192, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create metrics server task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

