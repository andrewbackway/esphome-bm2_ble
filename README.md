# esphome-bm2_ble

Updated external component that uses ESPhome's `ble_client` for connection management.
Top-level integration uses `ble_client` and the component receives a reference to it via `ble_client_id`.

Example YAML shows the `ble_client` block and the component configured with `blm2_ble_id`.

**Files included**:
- components/bm2_ble/__init__.py
- components/bm2_ble/sensor.py
- components/bm2_ble/text_sensor.py
- components/bm2_ble/manifest.json
- components/bm2_ble/bm2_ble.h
- components/bm2_ble/bm2_ble.cpp
- bm2_example.yaml
- bm2_example_test.yaml

## Board & ESPhome Version
- This repository includes example setup for the **Lolin S3 Pro** (ESP32-S3) board.
- The examples are configured to require **ESPhome 2025.11.0** and use the **ESP-IDF** framework.

See `COPILOT.md` for full developer instructions (esphome CLI setup, build commands, and debugging tips).
