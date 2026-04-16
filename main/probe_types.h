/*
 * probe_types.h - 探测结果类型定义
 *
 * 此头文件定义了网络探测的结果结构体以及相关类型和函数声明。
 *
 * 1. 探测结果结构体 probe_result_t
 *    包含探测是否成功、持续时间、状态码、错误信息等字段
 *    以及一个联合体，根据探测类型存储不同的详细数据
 *
 * 2. 函数指针类型 probe_func_t
 *    指向具体的探测执行函数
 *
 * 3. 探测执行函数声明
 *    定义了各种协议的探测实现函数
 */

#ifndef PROBE_TYPES_H
#define PROBE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "config_manager.h"

typedef struct {
    bool success;                // 探测是否成功
    uint32_t duration_ms;        // 探测总耗时(毫秒)
    int status_code;             // HTTP状态码或错误码
    char error_msg[128];         // 错误信息字符串
    
    union {
        struct {
            uint32_t connect_time_ms;  // 连接时间(毫秒)
            uint32_t tls_time_ms;     // TLS握手时间(毫秒)
            uint32_t ttfb_ms;         // Time to First Byte(毫秒)
            int http_status;          // HTTP状态码
        } http;
        
        struct {
            uint32_t connect_time_ms;  // 连接时间(毫秒)
            uint32_t tls_time_ms;     // TLS握手时间(毫秒)
        } tcp;
        
        struct {
            uint32_t resolve_time_ms;  // 域名解析时间(毫秒)
            char resolved_ip[16];      // 解析出的IP地址
        } dns;
        
        struct {
            uint32_t connect_time_ms;  // 连接时间(毫秒)
            uint32_t tls_time_ms;     // TLS握手时间(毫秒)
            uint32_t handshake_time_ms; // WebSocket握手时间(毫秒)
        } ws;
    } details;
} probe_result_t;

typedef probe_result_t (*probe_func_t)(const probe_target_t *target);

probe_result_t probe_http_execute(const probe_target_t *target);
probe_result_t probe_https_execute(const probe_target_t *target);
probe_result_t probe_tcp_execute(const probe_target_t *target);
probe_result_t probe_tcp_tls_execute(const probe_target_t *target);
probe_result_t probe_dns_execute(const probe_target_t *target);
probe_result_t probe_ws_execute(const probe_target_t *target);
probe_result_t probe_wss_execute(const probe_target_t *target);

#endif
