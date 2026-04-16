/*
 * wifi_config_server.h - WiFi 配置门户 HTTP 服务器接口
 *
 * 在 AP 模式下运行的 HTTP 服务器，提供 Web 配置页面。
 * 用户通过该页面扫描可用 WiFi、输入密码并保存到 NVS。
 */

#ifndef WIFI_CONFIG_SERVER_H
#define WIFI_CONFIG_SERVER_H

#include "esp_err.h"

/**
 * @brief 启动配置门户 HTTP 服务器
 *
 * 注册 GET /, GET /scan, POST /save 三个路由。
 * 如果服务器已在运行则直接返回。
 *
 * @return ESP_OK 启动成功
 */
esp_err_t wifi_config_server_start(void);

/**
 * @brief 停止配置门户 HTTP 服务器
 *
 * @return ESP_OK 停止成功
 */
esp_err_t wifi_config_server_stop(void);

#endif
