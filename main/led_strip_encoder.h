/*
 * led_strip_encoder.h - WS2812 RMT 编码器（XIAO ESP32C6 专用）
 *
 * 版权所有 (C) 2026 ESP32 Blackbox 项目
 *
 * 本程序为自由软件，遵循 MIT 许可证分发。
 */

#if !defined(LED_STRIP_ENCODER_H)
#define LED_STRIP_ENCODER_H

#if CONFIG_IDF_TARGET_ESP32C6

#include "driver/rmt_tx.h"

/**
 * @brief LED 灯带编码器结构体
 *
 * 基于 RMT（红外遥控单元）外设的 WS2812B 编码器实现。
 * 将 RGB 像素数据转换为 RMT 符号序列，采用 GRB 像素顺序。
 */
typedef struct {
    rmt_encoder_t base;                /* 基类编码器 */
    rmt_encoder_handle_t bytes_encoder; /* 字节编码器 */
    rmt_encoder_handle_t copy_encoder;  /* 复制编码器 */
    int state;                         /* 编码状态机状态 */
    rmt_symbol_word_t reset_code;       /* 复位信号的 RMT 符号 */
} led_strip_encoder_t;

/**
 * @brief 创建 WS2812 RMT 编码器
 *
 * @param channel_config RMT TX 通道配置
 * @param ret_encoder 输出：创建的编码器句柄
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t led_strip_encoder_new(const rmt_tx_channel_config_t *channel_config,
                                 rmt_encoder_handle_t *ret_encoder);

#endif /* CONFIG_IDF_TARGET_ESP32C6 */

#endif /* LED_STRIP_ENCODER_H */
