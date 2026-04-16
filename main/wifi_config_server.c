/**
 * wifi_config_server.c - WiFi 配置门户 HTTP 服务器实现
 *
 * 在 AP 模式下运行的 HTTP 服务器，提供 Web 配置页面。
 * 用户通过该页面扫描可用 WiFi、选择网络、输入密码。
 * 凭证保存到 NVS 后设备自动重启进入 STA 模式。
 *
 * 路由说明：
 * - GET  /      → 返回内嵌的暗色主题配置页面 HTML
 * - GET  /scan  → 扫描可见 WiFi 网络，返回 JSON 数组
 * - POST /save  → 接收 JSON {ssid, password}，保存到 NVS，重启设备
 */
#include "wifi_config_server.h"
#include "config_manager.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "cJSON.h"

static const char *TAG = "CFG_SRV";
static httpd_handle_t s_server = NULL;

/* --------------------------------------------------------------- */
/*  内嵌暗色主题配置页面 (单字符串, ~2.5KB)                       */
/*  包含扫描、选择、保存功能的完整 HTML+CSS+JS                     */
/* --------------------------------------------------------------- */
static const char CONFIG_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32 Blackbox</title><style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;"
"min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}"
".c{background:#16213e;border-radius:12px;padding:28px;width:100%;max-width:380px}"
"h1{text-align:center;font-size:1.3em;color:#e94560;margin-bottom:4px}"
"p.sub{text-align:center;color:#8892b0;font-size:.85em;margin-bottom:18px}"
"label{display:block;margin:14px 0 4px;font-size:.82em;color:#8892b0;font-weight:600}"
"input{width:100%;padding:11px;border:1px solid #233554;border-radius:8px;"
"background:#0f3460;color:#eee;font-size:15px;outline:none}"
"input:focus{border-color:#e94560}"
".btn{width:100%;padding:13px;border:none;border-radius:8px;font-size:15px;"
"cursor:pointer;font-weight:600;transition:background .2s}"
".bp{background:#e94560;color:#fff;margin-top:18px}"
".bp:hover{background:#c73650}"
".bs{background:#233554;color:#8892b0;margin-top:6px;font-size:13px;padding:10px}"
".bs:disabled{opacity:.5;cursor:not-allowed}"
".msg{padding:10px;border-radius:8px;margin-top:14px;font-size:.88em;display:none}"
".ok{background:#0f9b58;color:#fff;display:block}"
".er{background:#e94560;color:#fff;display:block}"
"#nets{max-height:180px;overflow-y:auto;margin-top:6px}"
".ni{padding:9px 11px;background:#0f3460;border-radius:6px;margin-bottom:3px;"
"cursor:pointer;display:flex;justify-content:space-between;font-size:.88em}"
".ni:hover{background:#1a4a8a}"
".ni.sel{background:#e94560}"
".ni .r{font-size:.72em;color:#8892b0}"
".ni.sel .r{color:rgba(255,255,255,.7)}"
"</style></head><body><div class='c'>"
"<h1>ESP32 Blackbox</h1>"
"<p class='sub'>WiFi Configuration</p>"
"<div id='m' class='msg'></div>"
"<button class='btn bs' id='sb' onclick='scan()'>Scan Networks</button>"
"<div id='nets'></div>"
"<form id='f' onsubmit='save(event)'>"
"<label>SSID</label>"
"<input type='text' id='s' required placeholder='WiFi name'>"
"<label>Password</label>"
"<input type='password' id='p' placeholder='WiFi password'>"
"<button class='btn bp' type='submit'>Save &amp; Restart</button>"
"</form></div>"
"<script>"
"function scan(){var b=document.getElementById('sb'),n=document.getElementById('nets');"
"b.disabled=true;b.textContent='Scanning...';n.innerHTML='';"
"var x=new XMLHttpRequest();x.onload=function(){b.disabled=false;"
"b.textContent='Scan Networks';if(x.status!==200){msg('Scan failed','er');return}"
"var a=JSON.parse(x.responseText);a.sort(function(a,b){return b.rssi-a.rssi});"
"n.innerHTML='';a.forEach(function(ap){if(!ap.ssid)return;"
"var d=document.createElement('div');d.className='ni';"
"d.innerHTML='<span>'+esc(ap.ssid)+'</span><span class=\"r\">'"
"+ap.rssi+'dBm'+(ap.auth>0?' 🔒':'')+'</span>';"
"d.onclick=function(){document.getElementById('s').value=ap.ssid;"
"document.querySelectorAll('.ni').forEach(function(e){e.classList.remove('sel')});"
"d.classList.add('sel')};n.appendChild(d)});"
"if(!a.length)msg('No networks found','er')};"
"x.onerror=function(){b.disabled=false;b.textContent='Scan Networks';msg('Scan failed','er')};"
"x.open('GET','/scan');x.send()}"
"function save(e){e.preventDefault();var s=document.getElementById('s').value,"
"p=document.getElementById('p').value;if(!s){msg('Enter SSID','er');return}"
"var x=new XMLHttpRequest();x.onload=function(){if(x.status===200)"
"{msg('Saved! Restarting...','ok');document.getElementById('f').style.display='none'"
";setTimeout(function(){location.reload()},5000)}else msg('Save failed','er')};"
"x.onerror=function(){msg('Network error','er')};"
"x.open('POST','/save',true);x.setRequestHeader('Content-Type','application/json');"
"x.send(JSON.stringify({ssid:s,password:p}))}"
"function msg(t,c){var m=document.getElementById('m');m.textContent=t;m.className='msg '+c}"
"function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML}"
"</script></body></html>";

/* --------------------------------------------------------------- */
/*  GET / - 返回配置页面 HTML                                      */
/* --------------------------------------------------------------- */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, CONFIG_HTML, -1);   /* -1 = use strlen */
}

/* --------------------------------------------------------------- */
/*  GET /scan - 扫描可见 WiFi 网络，返回 JSON 数组                  */
/*  返回字段: ssid, rssi (信号强度dBm), auth (认证模式)             */
/* --------------------------------------------------------------- */
static esp_err_t scan_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_cfg = {
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .show_hidden = false,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    uint16_t num = 0;
    esp_wifi_scan_get_ap_num(&num);

    wifi_ap_record_t *records = calloc(num, sizeof(wifi_ap_record_t));
    if (!records) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    esp_wifi_scan_get_ap_records(&num, records);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < num; i++) {
        /* skip empty SSIDs */
        if (records[i].ssid[0] == '\0') continue;
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", records[i].rssi);
        cJSON_AddNumberToObject(ap, "auth", records[i].authmode);
        cJSON_AddItemToArray(root, ap);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    free(json);
    free(records);
    cJSON_Delete(root);
    return ESP_OK;
}

/* --------------------------------------------------------------- */
/*  POST /save - 保存 WiFi 凭证到 NVS，然后重启设备                 */
/*  请求体: JSON {"ssid":"...", "password":"..."}                   */
/* --------------------------------------------------------------- */
static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *j_pass = cJSON_GetObjectItem(root, "password");

    if (!j_ssid || !cJSON_IsString(j_ssid) || j_ssid->valuestring[0] == '\0') {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    const char *ssid = j_ssid->valuestring;
    const char *pass = (j_pass && cJSON_IsString(j_pass)) ? j_pass->valuestring : "";

    esp_err_t err = config_manager_wifi_save(ssid, pass);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
        return ESP_FAIL;
    }

    const char resp[] = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    /* give the TCP stack a moment to flush the response */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    /* unreachable */
    return ESP_OK;
}

/* --------------------------------------------------------------- */
/*  公共 API                                                        */
/* --------------------------------------------------------------- */

/**
 * @brief 启动配置门户 HTTP 服务器
 *
 * 创建 HTTP 服务器并注册三个路由处理函数。
 * 如果服务器已在运行则直接返回成功。
 *
 * @return ESP_OK 启动成功，ESP_FAIL 启动失败
 */
esp_err_t wifi_config_server_start(void)
{
    if (s_server) {
        return ESP_OK;
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 3;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "failed to start HTTP server");
        return ESP_FAIL;
    }

    const httpd_uri_t uris[] = {
        { .uri = "/",    .method = HTTP_GET,  .handler = root_handler,  .user_ctx = NULL },
        { .uri = "/scan", .method = HTTP_GET,  .handler = scan_handler,  .user_ctx = NULL },
        { .uri = "/save", .method = HTTP_POST, .handler = save_handler,  .user_ctx = NULL },
    };
    for (int i = 0; i < 3; i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    ESP_LOGI(TAG, "config portal started on port 80");
    return ESP_OK;
}

esp_err_t wifi_config_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    return ESP_OK;
}
