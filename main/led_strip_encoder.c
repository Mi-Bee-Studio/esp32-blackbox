/*
 * led_strip_encoder.c - WS2812 RMT 编码器（XIAO ESP32C6 专用）
 *
 * 版权所有 (C) 2026 ESP32 Blackbox 项目
 *
 * 本程序为自由软件，遵循 MIT 许可证分发。
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32C6

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

static const char *TAG = "LED_ENCODER";

/* WS2812 时序参数（RMT 滴答周期由通道分辨率决定，通常为 12.5ns） */
#define WS2812_T0H_CYCLES  (0.3e-6)   /* 0 码高电平时间：0.3us */
#define WS2812_T0L_CYCLES  (0.9e-6)   /* 0 码低电平时间：0.9us */
#define WS2812_T1H_CYCLES  (0.9e-6)   /* 1 码高电平时间：0.9us */
#define WS2812_T1L_CYCLES  (0.3e-6)   /* 1 码低电平时间：0.3us */
#define WS2812_RESET_US    50         /* 复位信号持续时间：>=50us */

/* 编码器 encode 函数属性（放入 IRAM 以减少延迟） */
#define RMT_ENCODER_FUNC_ATTR IRAM_ATTR

/**
 * @brief 复合编码器的 encode 回调
 *
 * 状态机流程：
 *   状态 0 → 发送复位信号（copy 编码器）
 *   状态 1 → 发送像素数据（bytes 编码器）
 */
RMT_ENCODER_FUNC_ATTR
static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                    const void *primary_data, size_t data_size,
                                    rmt_encode_state_t *ret_state)
{
    led_strip_encoder_t *led_encoder = __containerof(encoder, led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_encoder->state) {
    case 0: /* 发送复位信号 */
        encoded_symbols += copy_encoder->encode(copy_encoder, channel,
                                                 &led_encoder->reset_code,
                                                 sizeof(led_encoder->reset_code),
                                                 &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; /* 切换到像素数据编码 */
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
        /* fall through */
    case 1: /* 发送像素数据 */
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel,
                                                  primary_data, data_size,
                                                  &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET; /* 回到初始状态 */
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    }

out:
    *ret_state = state;
    return encoded_symbols;
}

/**
 * @brief 复合编码器的 reset 回调
 */
static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    led_strip_encoder_t *led_encoder = __containerof(encoder, led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = 0;
    return ESP_OK;
}

/**
 * @brief 复合编码器的 del 回调，释放子编码器和自身内存
 */
static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    led_strip_encoder_t *led_encoder = __containerof(encoder, led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

/**
 * @brief 创建 WS2812 复合 RMT 编码器
 *
 * 内部创建 bytes 编码器（处理像素数据）和 copy 编码器（处理复位信号），
 * 组合为一个状态机式的复合编码器。
 *
 * @param config RMT TX 通道配置（用于获取分辨率）
 * @param ret_encoder 输出：创建的编码器句柄
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t led_strip_encoder_new(const rmt_tx_channel_config_t *config,
                                 rmt_encoder_handle_t *ret_encoder)
{
    if (ret_encoder == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    led_strip_encoder_t *led_encoder = calloc(1, sizeof(led_strip_encoder_t));
    if (led_encoder == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* 根据通道分辨率计算各时序参数的滴答数 */
    uint32_t resolution = 0;
    if (config) {
        resolution = config->resolution_hz;
    }
    if (resolution == 0) {
        resolution = 10000000; /* 默认 10MHz */
    }

    /* 创建字节编码器（像素数据 → RMT 符号） */
    rmt_bytes_encoder_config_t bytes_config = {
        .bit0 = {
            .level0 = 0,
            .duration0 = (uint32_t)(resolution * WS2812_T0L_CYCLES),
            .level1 = 1,
            .duration1 = (uint32_t)(resolution * WS2812_T0H_CYCLES),
        },
        .bit1 = {
            .level0 = 0,
            .duration0 = (uint32_t)(resolution * WS2812_T1L_CYCLES),
            .level1 = 1,
            .duration1 = (uint32_t)(resolution * WS2812_T1H_CYCLES),
        },
        .flags.msb_first = 1, /* WS2812 位顺序：G7...G0R7...R0B7...B0 */
    };

    esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &led_encoder->bytes_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建字节编码器失败");
        free(led_encoder);
        return ret;
    }

    /* 创建复制编码器（复位信号） */
    rmt_copy_encoder_config_t copy_config = {};
    ret = rmt_new_copy_encoder(&copy_config, &led_encoder->copy_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建复制编码器失败");
        rmt_del_encoder(led_encoder->bytes_encoder);
        free(led_encoder);
        return ret;
    }

    /* 计算复位信号的 RMT 符号 */
    uint32_t reset_ticks = resolution / 1000000 * WS2812_RESET_US / 2;
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };

    /* 注册编码器虚表 */
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    led_encoder->base.del = rmt_del_led_strip_encoder;

    *ret_encoder = &led_encoder->base;

    ESP_LOGI(TAG, "WS2812 编码器创建成功");

    return ESP_OK;
}

#endif /* CONFIG_IDF_TARGET_ESP32C6 */
