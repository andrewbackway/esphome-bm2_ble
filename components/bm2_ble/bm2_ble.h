#pragma once
#include "esphome.h"
#include "esphome/components/ble_client/ble_client.h"
#include <mbedtls/aes.h>

namespace esphome {
namespace bm2_ble {

class BM2BLEComponent : public Component {
 public:
  BM2BLEComponent() {}

  void setup() override;
  void loop() override;

  void set_ble_client(ble_client::BLEClient *client) { ble_client_ = client; }
  void set_update_interval(uint32_t sec) { update_interval_ = sec * 1000U; }

  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_battery_sensor(sensor::Sensor *s) { battery_sensor_ = s; }
  void set_status_text(text_sensor::TextSensor *t) { status_text_ = t; }

  void send_command(const std::vector<uint8_t> &cmd);

 protected:
  ble_client::BLEClient *ble_client_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_sensor_{nullptr};
  text_sensor::TextSensor *status_text_{nullptr};

  uint32_t update_interval_{10000}; // ms
  uint32_t last_update_{0};

  static constexpr uint8_t AES_KEY[16] = {
      0x6C,0x65,0x61,0x67,0x65,0x6E,0x64,0xFF,0xFE,0x31,0x38,0x38,0x32,0x34,0x36,0x36
  };

  void decrypt_and_handle(const std::vector<uint8_t>& data);
  void handle_voltage_hex(const std::string &hex);
  std::string bytes_to_hex(const uint8_t *data, size_t len);

  void ensure_subscription();

  bool subscribed_{false};
};
}  // namespace bm2_ble
}  // namespace esphome
