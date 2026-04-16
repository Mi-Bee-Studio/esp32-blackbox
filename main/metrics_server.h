/* 
 * Prometheus 指标服务器头文件
 * 
 * 定义了 Prometheus 指标服务器的 API 接口。
 * 提供启动指标服务器的函数声明。
 */

#ifndef METRICS_SERVER_H
#define METRICS_SERVER_H

#include "esp_err.h"

esp_err_t metrics_server_start(void);

#endif
