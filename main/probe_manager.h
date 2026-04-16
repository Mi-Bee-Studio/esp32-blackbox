/*
 * probe_manager.h - 探测任务调度器接口
 *
 * 管理网络探测任务的生命周期，包括初始化、启动和结果查询。
 * 探测任务在独立 FreeRTOS 任务中运行，循环执行所有配置的探测目标。
 */

#ifndef PROBE_MANAGER_H
#define PROBE_MANAGER_H

#include "esp_err.h"
#include "probe_types.h"

/**
 * @brief 初始化探测管理器
 *
 * 清零探测结果数组。
 *
 * @return ESP_OK 初始化成功
 */
esp_err_t probe_manager_init(void);

/**
 * @brief 启动探测任务
 *
 * 创建独立的 FreeRTOS 任务（16KB 栈，优先级 5），
 * 循环执行所有配置的探测目标。
 *
 * @return ESP_OK 启动成功，ESP_FAIL 任务创建失败
 */
esp_err_t probe_manager_start(void);

/**
 * @brief 获取探测结果数组
 *
 * @param count 输出参数，返回探测目标数量
 * @return 探测结果数组指针（内部静态存储，不要释放）
 */
const probe_result_t* probe_manager_get_results(uint8_t *count);

/**
 * @brief 获取探测目标数组
 *
 * @param count 输出参数，返回探测目标数量
 * @return 探测目标数组指针（内部静态存储，不要释放）
 */
const probe_target_t* probe_manager_get_targets(uint8_t *count);

#endif
