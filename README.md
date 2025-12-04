# esphome-bm2_ble

ESPHome external component for BM2 Battery Monitor using BLE communication. This component uses ESPHome's `ble_client` for connection management and follows a type-based entity configuration pattern.

## Features

- **Voltage Monitoring** - Real-time battery voltage measurement
- **Battery Percentage** - State of charge (0-100%)
- **Status Detection** - Battery status (normal, weak, very weak, charging)
- **Binary Sensors** - Charging state and weak battery indicators
- **Encrypted Communication** - AES-128-CBC encryption for all BLE data
- **Type-based Configuration** - Clean, flexible sensor configuration using `type` field

## Board & ESPHome Version

- Compatible with **ESP32-S3** boards (tested on Lolin S3 Pro)
- Requires **ESPHome 2025.11.0** or newer
- Uses **ESP-IDF** framework for native BLE support

## Installation

Add this external component to your ESPHome configuration:

```yaml
external_components:
  - source: github://andrewbackway/esphome-bm2_ble@main
```

## Configuration

### Basic Setup

```yaml
esp32:
  board: lolin_s3_pro
  framework:
    type: esp-idf

esp32_ble_tracker:

ble_client:
  - mac_address: "XX:XX:XX:XX:XX:XX"
    id: bm2_ble_client

bm2_ble:
  id: bm2_ble_hub
  ble_client_id: bm2_ble_client
```

### Available Sensor Types

#### Numeric Sensors (`sensor` platform)

| Type | Description | Unit | Decimals |
|------|-------------|------|----------|
| `voltage` | Battery voltage | V | 2 |
| `battery` | Battery percentage | % | 0 |

#### Text Sensors (`text_sensor` platform)

| Type | Description | Values |
|------|-------------|--------|
| `status` | Battery status | normal, weak, very weak, charging, unknown |

#### Binary Sensors (`binary_sensor` platform)

| Type | Description | On When |
|------|-------------|---------|
| `charging` | Charging indicator | Battery is charging |
| `weak_battery` | Weak battery alert | Battery is weak or very weak |

### Example Configuration

```yaml
sensor:
  - platform: bm2_ble
    bm2_ble_id: bm2_ble_hub
    type: voltage
    name: "BM2 Voltage"
    unit_of_measurement: "V"
    accuracy_decimals: 2

  - platform: bm2_ble
    bm2_ble_id: bm2_ble_hub
    type: battery
    name: "BM2 Battery Percent"
    unit_of_measurement: "%"
    accuracy_decimals: 0

text_sensor:
  - platform: bm2_ble
    bm2_ble_id: bm2_ble_hub
    type: status
    name: "BM2 Status"

binary_sensor:
  - platform: bm2_ble
    bm2_ble_id: bm2_ble_hub
    type: charging
    name: "BM2 Charging"

  - platform: bm2_ble
    bm2_ble_id: bm2_ble_hub
    type: weak_battery
    name: "BM2 Weak Battery"

```

## Protocol Details

The BM2 device communicates using:
- **Service UUID:** `0000FFF0-0000-1000-8000-00805F9B34FB`
- **Write Characteristic:** `0000FFF3-0000-1000-8000-00805F9B34FB`
- **Notify Characteristic:** `0000FFF4-0000-1000-8000-00805F9B34FB`
- **Encryption:** AES-128-CBC with fixed key and zero IV

For detailed protocol information, see [protocol.md](protocol.md).

## Architecture

This component follows the architecture pattern from [esphome-dometic_cfx_ble](https://github.com/andrewbackway/esphome-dometic_cfx_ble):

- **Type-based entity registration** - Sensors register with the hub component using `type` field
- **Dynamic entity maps** - Hub maintains maps of sensors by type for flexible updates
- **BLEClientNode integration** - Proper integration with ESPHome's BLE client infrastructure
- **Automatic decryption** - All notifications are automatically decrypted and routed

## Files Included

- `components/bm2_ble/__init__.py` - Component initialization and type definitions
- `components/bm2_ble/sensor.py` - Numeric sensor platform
- `components/bm2_ble/text_sensor.py` - Text sensor platform
- `components/bm2_ble/binary_sensor.py` - Binary sensor platform
- `components/bm2_ble/bm2_ble.h` - C++ header with component class
- `components/bm2_ble/bm2_ble.cpp` - C++ implementation with BLE and crypto
- `components/bm2_ble/manifest.json` - Component metadata
- `bm2_example.yaml` - Complete example configuration

## Development

See [COPILOT.md](COPILOT.md) for full developer instructions including:
- ESPHome CLI setup
- Build and flash commands
- Debugging tips
- Architecture overview

## Testing

A Python test script is available in `scripts/test.py` for validating BLE communication independently of ESPHome. This script demonstrates the encryption, decryption, and message parsing logic.

## License

MIT License - See LICENSE file for details.
