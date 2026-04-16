# ESP32 Blackbox 设计文档

## 设计目标

1. **轻量级**: 适用于资源受限的嵌入式设备 (ESP32-C3, 400KB SRAM)
2. **零配置**: 首次启动 AP 模式，Web 页面配置 WiFi，无需串口操作
3. **低功耗**: 间歇性探测减少能耗
4. **可靠性**: 稳定的网络探测和自动重连
5. **可观测性**: Prometheus 标准化指标输出

## 技术选型

### ESP-IDF v6.0

选择 ESP-IDF v6.0 的原因:
- mbedTLS v4 + PSA Crypto API，更现代的加密框架
- 更完善的 WiFi AP/STA 切换支持
- 更好的 RISC-V (ESP32-C3) 支持
- 原生支持 NVS 非易失性存储

### 目标硬件

| 项目 | 规格 |
|------|------|
| 芯片 | ESP32-C3 (QFN32) |
| 架构 | RISC-V 单核 160MHz |
| Flash | 4MB |
| SRAM | 400KB |
| WiFi | 802.11 b/g/n |
| BLE | Bluetooth 5 LE |

### 协议选择

| 协议 | 用途 | 依赖 |
|------|------|------|
| HTTP/HTTPS | Web 服务探测 | esp_http_client |
| TCP | 端口连通性测试 | lwip/sockets |
| TCP+TLS | TLS 加密连接探测 | lwip/sockets + mbedtls v4 |
| DNS | DNS 解析测试 | lwip/netdb |
| WebSocket | 实时通信测试 | lwip/sockets |
| WebSocket Secure | TLS 加密 WebSocket 测试 | lwip/sockets + mbedtls v4 |

## 核心设计决策

### 1. 首次配置: AP 模式

**设计**: 硬编码 WiFi vs AP 配置 vs BLE 配置

**选择**: AP 模式 Web 配置

**理由**:
- 无需电脑连接串口
- 手机/电脑浏览器即可配置
- 用户体验最好
- NVS 持久化凭据，配置一次永久有效

**流程**:
```
首次启动 → NVS 无凭据 → AP 模式 (SSID: ESP32_Blackbox)
                           → 用户连接 → 浏览器打开配置页
                           → 选择 WiFi → 输入密码 → 保存至 NVS
                           → 重启 → STA 模式自动连接
```

### 2. 探测执行模型

**设计**: 轮询模型 vs 事件模型

**选择**: 轮询模型

**理由**:
- 实现简单，可预测的探测间隔
- 适合间歇性探测场景
- 便于结果聚合和统计

### 3. 配置管理

**设计**: NVS 持久化 + 静态代码配置

**选择**: 混合方案

**已实现**:
- WiFi SSID/密码: NVS 持久化 (通过 AP 配置页面)
- 探测目标: 代码静态配置 (config_manager.c)

**计划**:
- 探测目标也支持 Web 动态配置
- 配置版本管理

### 4. Metrics 输出

**设计**: Push vs Pull

**选择**: Pull (Prometheus 模型)

**理由**:
- 设备作为 HTTP 服务器，被动响应
- 降低设备功耗和复杂性
- 便于 Prometheus 标准集成

### 5. 错误处理

采用静默失败策略:
- 探测失败不影响其他探测任务
- 错误信息存储在 result 中
- 通过 metrics 暴露错误状态

## 性能考量

### 内存使用

| 组件 | 估计内存 |
|------|----------|
| WiFi 栈 | ~40KB |
| LwIP | ~20KB |
| Probe Task | ~16KB 栈 (含 TLS) |
| Metrics Task | ~8KB 栈 |
| Config Server Task | ~4KB 栈 |
| TLS (mbedTLS v4 / PSA) | ~25KB |
| App 分区 | ~960KB (94% 使用) |

### 网络探测超时

合理的超时设置:
- HTTP/HTTPS: 10-30秒
- TCP: 5-10秒
- TCP+TLS/WSS: 10-15秒 (TLS 握手需要额外时间)
- DNS: 5秒

### 探测间隔

建议间隔:
- 关键服务: 30秒
- 普通服务: 1-5分钟
- 低优先级: 10分钟以上

## 安全性考虑

### WiFi 安全

- AP 模式使用 WPA2-PSK (SSID: `ESP32_Blackbox`, 密码: `12345678`)
- STA 模式使用 WPA2-PSK 连接用户 WiFi
- WiFi 凭据加密存储在 NVS 中

### TLS/SSL

- TCP+TLS 和 WSS 探测使用 mbedTLS v4 库
- TLS 通过 PSA Crypto API 自动管理随机数和密钥
- 支持证书验证 (可选, 通过 `verify_ssl` 字段控制)
- TLS 探测分阶段计时: TCP 连接耗时、TLS 握手耗时独立记录
- 支持 SNI (Server Name Indication) 扩展

### 配置页面安全

- AP 模式配置服务器仅在有客户端连接时运行
- 配置完成后自动重启进入 STA 模式，AP 关闭
- 密码通过 HTTPS 表单提交 (当前为 HTTP，局域网内使用)

### 输入验证

- 所有目标地址和端口经过长度检查
- URL 构造使用 snprintf 防止溢出
- WiFi 凭据长度校验

## 扩展性设计

### 添加新协议

1. 在 `config_manager.h` 添加类型枚举
2. 实现 `probe_xxx_execute()` 函数
3. 在 `probe_manager.c` 的 switch 中添加 case 分支
4. 更新 `metrics_server.c` 添加协议名称映射

### 持久化配置

已实现:
- NVS 存储 WiFi SSID 和密码
- AP 配置 Web 服务器

计划实现:
- NVS 存储探测目标列表
- WebSocket API 动态配置
- 配置版本管理

## 监控指标设计

### Prometheus 指标命名规范

遵循 Prometheus 最佳实践:
- 使用下划线分隔
- 包含目标标签
- 包含协议类型标签

### 指标类型

| 指标 | 类型 | 说明 |
|------|------|------|
| probe_duration_seconds | Gauge | 探测耗时 |
| probe_success | Gauge | 成功标志 |
| probe_status_code | Gauge | 响应码 |
| probe_tls_handshake_seconds | Gauge | TLS 握手耗时 (仅 tcp_tls/wss) |

### 标签设计

```promql
probe_duration_seconds{target="example.com", port="443", type="https"}
```

标签组合确保唯一性。

## 未来规划

### 短期

- [x] 完成 WSS 探测实现
- [x] 完成 TCP+TLS 探测实现
- [x] 添加 TLS 握手耗时指标
- [x] NVS WiFi 凭据持久化
- [x] AP 模式 Web 配置页面
- [x] ESP-IDF v6.0 迁移
- [ ] 优化探测并发
- [ ] 清理调试日志释放 Flash 空间

### 中期

- [ ] 探测目标 Web 动态配置
- [ ] OTA 固件更新
- [ ] 历史数据本地存储
- [ ] mDNS 服务发现

### 长期

- [ ] MQTT 协议支持
- [ ] 多设备协同
- [ ] 边缘计算能力
- [ ] 告警通知 (Webhook / 邮件)
