/*
 * status_led.c - LED 状态指示模块
 *
 * 通过板载 LED 显示设备当前状态。
 * ESP32-C3: GPIO8 单色 LED 闪烁模式（低电平点亮）。
 * ESP32-C6: GPIO15 WS2812 RGB LED 颜色+闪烁模式。
 *
 * 版权所有 (C) 2026 ESP32 Blackbox 项目
 *
 * 本程序为自由软件，遵循 MIT 许可证分发。
 */

#include "sdkconfig.h"

#ifdef CONFIG_ESP_STATUS_LED

#include "status_led.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"

#if CONFIG_IDF_TARGET_ESP32C6
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#endif

#include "esp_wifi.h"
#include "esp_netif.h"

static const char *TAG = "STATUS_LED";

/* 静态全局变量 */
static status_led_state_t s_current_state = STATUS_LED_INIT;
static bool s_initialized = false;
static TaskHandle_t s_led_task_handle = NULL;
static portMUX_TYPE s_state_mutex = portMUX_INITIALIZER_UNLOCKED;

/* C6 专用变量 */
#if CONFIG_IDF_TARGET_ESP32C6
static rmt_channel_handle_t s_rmt_chan = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;
static uint8_t s_pixel_buf[3] = {0};  /* GRB: 绿、红、蓝 */
#endif

/* LED 任务栈大小和优先级 */
#define LED_TASK_STACK_SIZE  3072
#define LED_TASK_PRIORITY    (tskIDLE_PRIORITY + 1)

/* 通用闪烁参数（毫秒） */
#define BLINK_FAST_MS        50    /* 快闪：50ms 亮 / 50ms 灭 */
#define BLINK_MEDIUM_MS      100   /* 中速闪：100ms 亮 / 100ms 灭 */
#define BLINK_SLOW_MS        250   /* 慢闪：250ms 亮 / 250ms 灭 */
#define PAUSE_LONG_MS        1000  /* 长暂停 */

/* ── C3 专用辅助函数 ── */

#if CONFIG_IDF_TARGET_ESP32C3

/* 低电平点亮（active-low） */
static void c3_led_on(void) { gpio_set_level(STATUS_LED_GPIO_NUM, 0); }
static void c3_led_off(void) { gpio_set_level(STATUS_LED_GPIO_NUM, 1); }

/**
 * @brief C3 LED 闪烁
 * @param on_ms  亮灯持续时间（毫秒）
 * @param off_ms 灭灯持续时间（毫秒）
 * @param count  闪烁次数
 */
static void c3_blink(uint32_t on_ms, uint32_t off_ms, int count)
{
    for (int i = 0; i < count; i++) {
        c3_led_on();
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        c3_led_off();
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

#endif /* CONFIG_IDF_TARGET_ESP32C3 */

/* ── C6 专用辅助函数 ── */

#if CONFIG_IDF_TARGET_ESP32C6

/* 颜色定义（低亮度，避免刺眼） */
#define COLOR_R_OFF    0
#define COLOR_G_OFF    0
#define COLOR_B_OFF    0

#define COLOR_R_GREEN  0
#define COLOR_G_GREEN  30
#define COLOR_B_GREEN  0

#define COLOR_R_RED    30
#define COLOR_G_RED    0
#define COLOR_B_RED    0

#define COLOR_R_BLUE   0
#define COLOR_G_BLUE   0
#define COLOR_B_BLUE   30

#define COLOR_R_YELLOW 30
#define COLOR_G_YELLOW 30
#define COLOR_B_YELLOW 0

#define COLOR_R_PURPLE 20
#define COLOR_G_PURPLE 0
#define COLOR_B_PURPLE 30

/**
 * @brief 设置 WS2812 RGB 颜色（GRB 顺序）
 * @param r 红色分量（0-255）
 * @param g 绿色分量（0-255）
 * @param b 蓝色分量（0-255）
 */
static void c6_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    s_pixel_buf[0] = g;  /* GRB 顺序：绿 */
    s_pixel_buf[1] = r;  /* 红 */
    s_pixel_buf[2] = b;  /* 蓝 */
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,  /* 不循环 */
    };
    rmt_transmit(s_rmt_chan, s_led_encoder, s_pixel_buf, sizeof(s_pixel_buf), &tx_config);
    rmt_tx_wait_all_done(s_rmt_chan, pdMS_TO_TICKS(100));
}

/* 关闭 WS2812 LED */
static void c6_set_rgb_off(void)
{
    c6_set_color(COLOR_R_OFF, COLOR_G_OFF, COLOR_B_OFF);
}

/**
 * @brief C6 RGB LED 闪烁
 * @param r      红色分量
 * @param g      绿色分量
 * @param b      蓝色分量
 * @param on_ms  亮灯持续时间（毫秒）
 * @param off_ms 灭灯持续时间（毫秒）
 * @param count  闪烁次数
 */
static void c6_blink(uint8_t r, uint8_t g, uint8_t b,
                     uint32_t on_ms, uint32_t off_ms, int count)
{
    for (int i = 0; i < count; i++) {
        c6_set_color(r, g, b);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        c6_set_rgb_off();
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

#endif /* CONFIG_IDF_TARGET_ESP32C6 */

/* ── 事件处理函数 ── */

/**
 * @brief WiFi 事件处理
 *
 * 监听 WiFi 状态变化并切换 LED 模式。
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        status_led_set_state(STATUS_LED_STA_CONNECTING);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        status_led_set_state(STATUS_LED_DISCONNECTED);
    } else if (event_id == WIFI_EVENT_AP_START) {
        status_led_set_state(STATUS_LED_AP_MODE);
    }
}

/**
 * @brief IP 事件处理
 *
 * 获取到 IP 地址时切换到已连接状态。
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        status_led_set_state(STATUS_LED_CONNECTED);
    }
}

/* ── LED 任务 ── */

/**
 * @brief LED 状态指示任务
 *
 * 根据当前状态驱动板载 LED 显示对应的闪烁模式。
 * C3 使用 GPIO 控制单色 LED，C6 使用 RMT+WS2812 控制 RGB LED。
 */
static void led_task(void *arg)
{
    ESP_LOGI(TAG, "LED 任务启动");

    while (1) {
        /* 读取当前状态（临界区保护） */
        status_led_state_t state;
        portENTER_CRITICAL(&s_state_mutex);
        state = s_current_state;
        portEXIT_CRITICAL(&s_state_mutex);

        switch (state) {
        case STATUS_LED_INIT:
            /* 初始化中：快闪 100ms（50ms 亮 + 50ms 灭） */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_FAST_MS, BLINK_FAST_MS, 1);
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_BLUE, COLOR_G_BLUE, COLOR_B_BLUE,
                     BLINK_FAST_MS, BLINK_FAST_MS, 1);
#endif
            break;

        case STATUS_LED_AP_MODE:
            /* AP 模式：慢闪 500ms（250ms 亮 + 250ms 灭） */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_SLOW_MS, BLINK_SLOW_MS, 1);
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_YELLOW, COLOR_G_YELLOW, COLOR_B_YELLOW,
                     BLINK_SLOW_MS, BLINK_SLOW_MS, 1);
#endif
            break;

        case STATUS_LED_STA_CONNECTING:
            /* STA 连接中：中速闪 200ms（100ms 亮 + 100ms 灭） */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_MEDIUM_MS, BLINK_MEDIUM_MS, 1);
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_BLUE, COLOR_G_BLUE, COLOR_B_BLUE,
                     BLINK_MEDIUM_MS, BLINK_MEDIUM_MS, 1);
#endif
            break;

        case STATUS_LED_CONNECTED:
            /* 已连接：常亮 */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_led_on();
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_set_color(COLOR_R_GREEN, COLOR_G_GREEN, COLOR_B_GREEN);
#endif
            /* 避免忙循环，长时间挂起 */
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        case STATUS_LED_DISCONNECTED:
            /* 已断开：快闪 100ms（50ms 亮 + 50ms 灭） */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_FAST_MS, BLINK_FAST_MS, 1);
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_RED, COLOR_G_RED, COLOR_B_RED,
                     BLINK_FAST_MS, BLINK_FAST_MS, 1);
#endif
            break;

        case STATUS_LED_CONNECTION_FAILED:
            /* 连接失败：快闪 3 次后暂停 1 秒 */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_FAST_MS, BLINK_FAST_MS, 3);
            c3_led_off();
            vTaskDelay(pdMS_TO_TICKS(PAUSE_LONG_MS));
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_RED, COLOR_G_RED, COLOR_B_RED,
                     BLINK_FAST_MS, BLINK_FAST_MS, 3);
            c6_set_rgb_off();
            vTaskDelay(pdMS_TO_TICKS(PAUSE_LONG_MS));
#endif
            break;

        case STATUS_LED_SELF_TEST:
            /* 自检中：C6 紫色快闪，C3 同 INIT 模式 */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_FAST_MS, BLINK_FAST_MS, 1);
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_PURPLE, COLOR_G_PURPLE, COLOR_B_PURPLE,
                     BLINK_FAST_MS, BLINK_FAST_MS, 1);
#endif
            break;

        case STATUS_LED_CONFIG_RELOAD:
            /* 配置重载：闪烁 2 次后等待外部恢复状态 */
#if CONFIG_IDF_TARGET_ESP32C3
            c3_blink(BLINK_MEDIUM_MS, BLINK_MEDIUM_MS, 2);
#elif CONFIG_IDF_TARGET_ESP32C6
            c6_blink(COLOR_R_YELLOW, COLOR_G_YELLOW, COLOR_B_YELLOW,
                     BLINK_MEDIUM_MS, BLINK_MEDIUM_MS, 2);
#endif
            /* 闪烁完成后短暂等待，由调用方恢复之前的状态 */
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }

        /* 短暂让出 CPU，防止状态未变时占用过多时间片 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── 公共接口 ── */

esp_err_t status_led_init(void)
{
    /* 防止重复初始化 */
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;

#if CONFIG_IDF_TARGET_ESP32C3
    /* C3：配置 GPIO 为输出模式，初始高电平（LED 灭） */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(STATUS_LED_GPIO_NUM, 1);  /* 初始状态：LED 灭（低电平点亮） */

#elif CONFIG_IDF_TARGET_ESP32C6
    /* C6：配置 RMT TX 通道驱动 WS2812 */
    rmt_tx_channel_config_t rmt_config = {
        .gpio_num = STATUS_LED_GPIO_NUM,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  /* 10MHz 分辨率 */
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ret = rmt_new_tx_channel(&rmt_config, &s_rmt_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT TX 通道创建失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rmt_enable(s_rmt_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT 通道使能失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = led_strip_encoder_new(&rmt_config, &s_led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 编码器创建失败: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    /* 创建 LED 状态指示任务 */
    BaseType_t task_ret = xTaskCreate(led_task, "status_led",
                                      LED_TASK_STACK_SIZE, NULL,
                                      LED_TASK_PRIORITY, &s_led_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "LED 任务创建失败");
        return ESP_FAIL;
    }

    /* 注册 WiFi 事件处理 */
    esp_event_handler_instance_t wifi_inst = NULL;
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL, &wifi_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 事件注册失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 注册 IP 事件处理 */
    esp_event_handler_instance_t ip_inst = NULL;
    ret = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                               ip_event_handler, NULL, &ip_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IP 事件注册失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LED 状态指示器初始化完成");

    return ESP_OK;
}

void status_led_set_state(status_led_state_t state)
{
    if (!s_initialized) {
        return;
    }
    portENTER_CRITICAL(&s_state_mutex);
    s_current_state = state;
    portEXIT_CRITICAL(&s_state_mutex);
    ESP_LOGI(TAG, "LED 状态切换: %d", (int)state);
}

#endif /* CONFIG_ESP_STATUS_LED */
