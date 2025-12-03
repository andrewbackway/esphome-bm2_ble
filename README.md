# esphome-bm2_ble

Updated external component that uses ESPHome's `ble_client` for connection management.
Top-level integration uses `ble_client` and the component receives a reference to it via `ble_client_id`.

Example YAML shows the `ble_client` block and the component configured with `blm2_ble_id`.

**Files included**:
- custom_components/bm2_ble/__init__.py
- custom_components/bm2_ble/sensor.py
- custom_components/bm2_ble/text_sensor.py
- custom_components/bm2_ble/manifest.json
- custom_components/bm2_ble/bm2_ble.h
- custom_components/bm2_ble/bm2_ble.cpp
- example/bm2_example.yaml
