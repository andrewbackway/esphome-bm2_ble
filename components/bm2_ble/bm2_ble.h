#pragma once
#include "esphome.h"
#include <mbedtls/aes.h>

namespace esphome {
namespace bm2_ble {

class BM2BLEComponent : public Component, public UARTDevice {
 public:
  BM2BLEComponent(const std::string &mac) : mac_(mac) {}

  void setup() override;
  void loop() override;

  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_battery_sensor(sensor::Sensor *s) { battery_sensor_ = s; }
  void set_status_text(text_sensor::TextSensor *t) { status_text_ = t; }

  // Public helper to send encrypted command bytes (not used automatically)
  void send_command(const std::vector<uint8_t> &cmd);

 protected:
  std::string mac_;
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_sensor_{nullptr};
  text_sensor::TextSensor *status_text_{nullptr};

  // AES state
  static constexpr uint8_t AES_KEY[16] = {
      0x6C,0x65,0x61,0x67,0x65,0x6E,0x64,0xFF,0xFE,0x31,0x38,0x38,0x32,0x34,0x36,0x36
  };

  void decrypt_and_handle(const std::vector<uint8_t>& data);
  // parsing helpers
  void handle_voltage_hex(const std::string &hex);
  std::string bytes_to_hex(const uint8_t *data, size_t len);

  // Note: BLE client internals are implemented using NimBLE APIs available through ESP-IDF.
  void ensure_connected();
  void disconnect_ble();

  // internal flags
  bool connected_{false};
  unsigned long last_connect_attempt_{0};
};
}  // namespace bm2_ble
}  // namespace esphome
