# esphome-bm2_ble

ESPHome external component that connects to the BM2 Battery Monitor over BLE and exposes sensors:
- Voltage (float, V)
- Battery percentage (integer)
- Status (text)

Tested target hardware: Lolin S3 Pro (ESP32-S3)

## Installation

1. In your ESPHome YAML, add:

```yaml
external_components:
  - source: github://andrewbackway/esphome-bm2_ble@main
```

3. Reference the sensors as shown in `_example.yaml`.

## Notes

- The component uses AES-128 CBC with a fixed key and a zero IV as used by the BM2 device.
- The component uses NimBLE APIs; ensure your ESPHome build environment includes NimBLE support for ESP32/ESP-IDF.
- This is an external component and may require tweaks to match your specific ESPHome and ESP-IDF setup. If you encounter compile issues, open an issue in the repo with the build log.

## Files

## License

MIT
