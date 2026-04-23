# ESP32 Blackbox

[English](README.md) | **中文**

## Overview

ESP32 Blackbox is a network probing device compatible with Prometheus blackbox_exporter architecture, featuring JSON configuration and Web UI management. Now supports on-demand probing functionality via `/probe` endpoint.

**Key Features**:
- **Zero-config startup**: Automatically enters AP mode on first boot with WiFi web configuration
- **Configuration-driven probing**: JSON + SPIFFS for modifying probe targets without recompilation
- **blackbox_exporter compatible**: `/probe` endpoint supporting Prometheus active scraping and on-demand probing
- **ICMP Ping probing**: Native socket implementation
- **Hot configuration reload**: Runtime config modification without restart
- **Web UI configuration management**: AP mode WiFi setup + STA mode probe management
- **Multi-protocol probing**: HTTP/HTTPS/TCP/TLS/DNS/ICMP/WebSocket (WS/WSS)
- **Dual target support**: ESP32-C3 and ESP32-C6 (WiFi 6)
- **On-demand probing**: `/probe` endpoint supports dynamic target configuration and real-time network testing

## Hardware Requirements

- ESP32-C3 development board (e.g., ESP32-C3 SuperMini)
- ESP32-C6 development board (e.g., Seeed Studio XIAO ESP32C6)
- WiFi network connection
- USB cable (for flashing)

## Software Dependencies

- ESP-IDF **v6.0** (mbedTLS v4 + PSA Crypto)
- Local component: cJSON (`components/json/`)

## Quick Start

### 1. Initial WiFi Configuration

Power on device → Connect to WiFi hotspot `ESP32_Blackbox` (password: `12345678`) → Open browser to `192.168.4.1` → Scan for WiFi networks → Enter credentials → Save and restart

Detailed steps see [Usage Guide](docs/en/usage.md#initial-ap-mode-setup)

### 2. Build and Flash

**Method 1: Python Build Script (Recommended)**

```bash
# ESP32-C6 Build
python build.py esp32c6 build

# ESP32-C6 Build and Flash
python build.py esp32c6 flash COM3

# ESP32-C3 Build and Flash + Monitor
python build.py esp32c3 monitor COM3
```

**Method 2: ESP-IDF Command Line (requires export.bat first)**

```bash
# First build - Set target chip
idf.py set-target esp32c6        # or esp32c3

# Configuration (optional)
idf.py menuconfig                # Configure AP SSID/password, retry count, self-test

# Build
idf.py build

# Flash
idf.py -p COM3 flash

# Flash + Serial Monitor
idf.py -p COM3 flash monitor     # Ctrl+] to exit monitor

# Clean
idf.py fullclean                  # Clean all build artifacts
```

**Method 3: Windows Batch Script**

```cmd
build_target.bat esp32c6 build          # Build only
build_target.bat esp32c6 flash COM3     # Build and flash
build_target.bat esp32c6 clean          # Clean and rebuild
build_target.bat esp32c3 monitor COM3   # Build, flash and monitor
```

### 3. Modify Probe Targets

Supports two configuration methods:

**Method 1: Web UI Configuration**

Open browser to `http://<deviceIP>/` → Edit JSON configuration → Save

**Method 2: JSON File Configuration**

Edit `/spiffs/blackbox.json` configuration file (accessible via Web UI or API)

Configuration file format:

```json
{
  "scrape_interval": 30,
  "metrics_port": 9090,
  "modules": {
    "http_2xx": {
      "prober": "http",
      "timeout": 10,
      "http": {
        "method": "GET",
        "valid_status_codes": [200]
      }
    },
    "icmp_ping": {
      "prober": "icmp",
      "timeout": 5,
      "icmp": {
        "packets": 3,
        "payload_size": 56
      }
    }
  },
  "targets": [
    {
      "name": "httpbin_http",
      "target": "httpbin.org"
    }
  ]
}
```

## Project Structure

```
esp32-blackbox/
├── main/
│   ├── wifi_manager.c/h        # WiFi connection management (AP/STA dual mode)
│   ├── web_server.c/h          # Unified web server (AP config + STA + on-demand probing)
│   ├── config_manager.c/h      # Configuration management + NVS storage + JSON SPIFFS
│   ├── probe_manager.c/h       # Probe task scheduling
│   ├── probe_types.h           # Probe type definitions
│   ├── probe_http.c            # HTTP/HTTPS probing
│   ├── probe_tcp.c             # TCP/TCP+TLS probing
│   ├── probe_dns.c             # DNS probing
│   ├── probe_icmp.c            # ICMP Ping probing
│   ├── probe_ws.c              # WebSocket/WSS probing
│   ├── metrics_server.c/h      # Prometheus metrics + /probe endpoint
│   ├── CMakeLists.txt          # Component registration
│   └── Kconfig.projbuild       # Menuconfig options
│   └── json/                   # Local cJSON component (v6.0 compatibility)
│       ├── cJSON.c
│       ├── cJSON.h
│       └── CMakeLists.txt
├── docs/                       # Documentation directory
│   ├── zh/                    # Chinese documentation
│   │   ├── architecture.md    # Architecture design
│   │   ├── design.md         # Design documentation
│   │   └── usage.md          # Usage guide
│   └── en/                    # English documentation
│       ├── architecture.md    # Architecture
│       ├── design.md         # Design
│       └── usage.md          # Usage
├── CMakeLists.txt              # Root project configuration
├── sdkconfig.defaults          # Default configuration
├── sdkconfig.defaults.esp32c3  # ESP32-C3 configuration
├── sdkconfig.defaults.esp32c6  # ESP32-C6 configuration
├── partitions.csv              # Custom partition table (with SPIFFS)
├── build.py                    # Python build script (recommended)
├── build_target.bat            # Windows batch build script
├── README.md                   # English README
├── README_CN.md                # Chinese README
└── AGENTS.md                   # Project documentation
```

## Documentation

For more documentation, see the [docs/](docs/) directory:

- [Architecture](docs/en/architecture.md) — Module division, startup flow, thread model
- [Design](docs/en/design.md) — Design decisions, performance considerations, security
- [Usage Guide](docs/en/usage.md) — Environment setup, configuration, flashing, troubleshooting

## Prometheus Integration

After the device connects to WiFi, access `http://<deviceIP>:9090/metrics` to get Prometheus metrics:
```
# HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
probe_success{target="httpbin_http",module="http_2xx"} 1

# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
probe_duration_seconds{target="httpbin_http",module="http_2xx"} 0.234

# HELP probe_http_status_code HTTP status code
# TYPE probe_http_status_code gauge
probe_http_status_code{target="httpbin_http",module="http_2xx"} 200
```

### Prometheus blackbox_exporter Scrape Configuration

```yaml
scrape_configs:
  - job_name: 'blackbox_http'
    metrics_path: /probe
    params:
      module: [http_2xx]
    static_configs:
      - targets: ['httpbin.org:80']
    relabel_configs:
      - source_labels: [__address__]
        target_label: __param_target
      - target_label: __address__
        replacement: 192.168.1.100:9090

  - job_name: 'esp32-blackbox'
    static_configs:
      - targets: ['192.168.1.100:9090']
```

## HTTP Endpoints

### Port 9090 (metrics_server)
- `GET /metrics` — Prometheus format metrics for all targets
- `GET /probe?target=X&module=Y&port=P` — Single synchronous probe (optional port), returns Prometheus format
- `GET /config` — Current JSON configuration
- `POST /reload` — Hot load SPIFFS configuration

### Port 80 (web_server)
**AP Mode:**
- `GET /` — WiFi configuration page
- `GET /scan` — WiFi scan results JSON
- `POST /save` — Save WiFi credentials to NVS

**STA Mode:**
- `GET /` — Configuration management dashboard
- `GET /api/status` — Device status JSON
- `POST /api/config` — Update configuration JSON
- `POST /api/reload` — Hot load configuration

## License

MIT

## On-Demand Probing Feature

### Overview

ESP32 Blackbox now supports on-demand probing functionality, allowing direct network connectivity testing through the `/probe` endpoint without modifying configuration files.

### Usage

**Basic syntax**:
```bash
GET /probe?target=<hostname/IP>&module=<module_name>&port=<port>
```

**Parameter description**:
• `target`: Target hostname or IP address (required)
• `module`: Probe module name (required)
• `port`: Target port (optional, determined by module by default)

**Supported modules**:
| Module | Protocol | Description |
--------|----------|-------------|
 `http_2xx` | HTTP/HTTPS | HTTP GET/POST probing with status code validation |
 `tcp` | TCP | TCP connection test |
 `dns` | DNS | DNS resolution test |
 `icmp_ping` | ICMP | ICMP Ping test |
 `ws` | WebSocket | WebSocket connection test |
 `wss` | WebSocket Secure | WebSocket + TLS connection test |

**Usage examples**:
```bash
# HTTP probe
curl "http://<deviceIP>:9090/probe?target=httpbin.org&module=http_2xx"

# TCP probe (specify port)
curl "http://<deviceIP>:9090/probe?target=example.com&module=tcp&port=443"

# DNS probe
curl "http://<deviceIP>:9090/probe?target=8.8.8.8&module=dns"
```

### Output Format

On-demand probing returns Prometheus format metrics:
```
# HELP probe_duration_seconds Duration of the probe in seconds
# TYPE probe_duration_seconds gauge
probe_duration_seconds{target="httpbin.org", module="http_2xx"} 0.234

# HELP probe_success Whether the probe succeeded
# TYPE probe_success gauge
probe_success{target="httpbin.org", module="http_2xx"} 1
```

### Advantages

✅ **No configuration files required**: Direct network testing via URL
✅ **Fast verification**: Immediate test results
✅ **Flexible parameters**: Support for any hostname, IP and port
✅ **Standardized output**: Prometheus format, easy integration
✅ **Debug-friendly**: Real-time network connectivity checking