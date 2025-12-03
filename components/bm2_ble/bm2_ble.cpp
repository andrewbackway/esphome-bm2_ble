// bm2_ble.cpp
#include "bm2_ble.h"
#include "esphome/core/log.h"
#include <esp_gattc_api.h>
#include <vector>
#include <cstring>
#include <mbedtls/aes.h>

namespace esphome {
namespace bm2_ble {

void BM2BLEComponent::setup() {
  ESP_LOGI(TAG, "BM2BLEComponent setup");
}

void BM2BLEComponent::loop() {
  if (!this->subscribed_ && this->parent_ != nullptr) {
    if (this->parent_->connected()) {
      this->ensure_subscription();
    }
  }
}

void BM2BLEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "BM2 BLE:");
  ESP_LOGCONFIG(TAG, "  Sensors: %d", (int)this->sensors_.size());
  ESP_LOGCONFIG(TAG, "  Binary Sensors: %d", (int)this->binary_sensors_.size());
  ESP_LOGCONFIG(TAG, "  Text Sensors: %d", (int)this->text_sensors_.size());
}

void BM2BLEComponent::ensure_subscription() {
  if (this->subscribed_) return;
  if (this->parent_ == nullptr || !this->parent_->connected()) {
    ESP_LOGW(TAG, "BLE client not connected");
    return;
  }

  // The BM2 device uses service 0xFFF0 with characteristics FFF3 (write) and FFF4 (notify).
  auto svc = this->parent_->get_service(0xfff0);
  if (svc == nullptr) {
    ESP_LOGW(TAG, "Service 0xfff0 not found");
    return;
  }
  auto notify_char = svc->get_characteristic(0xfff4);
  auto write_char = svc->get_characteristic(0xfff3);

  if (notify_char == nullptr) {
    ESP_LOGW(TAG, "Notify characteristic not found");
    return;
  }

  // store handle for comparing in gattc event handler
  this->notify_handle_ = notify_char->handle;
  this->write_handle_ = write_char ? write_char->handle : 0;

  // Request notification registration
  esp_err_t err = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(),
                                                   this->parent_->get_remote_bda(),
                                                   this->notify_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed: %d", err);
  }

  this->subscribed_ = true;
  ESP_LOGI(TAG, "Subscribed to BM2 notifications");
}

void BM2BLEComponent::decrypt_and_handle(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> in(data.begin(), data.end());
  size_t pad = (16 - (in.size() % 16)) % 16;
  if (pad) in.insert(in.end(), pad, 0);

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_dec(&ctx, AES_KEY, 128) != 0) {
    ESP_LOGE(TAG, "AES setkey_dec failed");
    mbedtls_aes_free(&ctx);
    return;
  }

  std::vector<uint8_t> out(in.size(), 0);
  uint8_t iv[16] = {0};
  int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, in.size(), iv, in.data(), out.data());
  mbedtls_aes_free(&ctx);
  if (rc != 0) {
    ESP_LOGE(TAG, "AES decrypt failed: %d", rc);
    return;
  }

  // convert to hex string and strip trailing 00 bytes
  std::string hex;
  hex.reserve(out.size() * 2);
  for (size_t i = 0; i < out.size(); ++i) {
    char buf[3];
    sprintf(buf, "%02x", out[i]);
    hex += buf;
  }
  while (hex.size() >= 2 && hex.substr(hex.size() - 2) == "00")
    hex.erase(hex.size() - 2);

  ESP_LOGD(TAG, "Decrypted HEX: %s", hex.c_str());
  this->parse_voltage_message(hex);
}

void BM2BLEComponent::parse_voltage_message(const std::string &hex) {
  // Check for special diagnostic messages
  if (hex.rfind("fefefe", 0) == 0) {
    ESP_LOGD(TAG, "Received fefefe diagnostic marker (charge test)");
    return;
  }

  if (hex.find("fffefe") != std::string::npos) {
    ESP_LOGD(TAG, "Received fffefe marker (crank test)");
    return;
  }

  // Parse standard voltage message (minimum 16 hex chars = 8 bytes)
  if (hex.size() >= 16) {
    auto parse_hex_substring = [&](size_t pos, size_t len, int &out) -> bool {
      if (pos + len > hex.size()) return false;
      out = 0;
      for (size_t i = pos; i < pos + len; ++i) {
        char c = hex[i];
        int val = 0;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else return false;
        out = (out << 4) + val;
      }
      return true;
    };

    int voltage_raw = 0;
    int status_raw = 0;
    int battery_raw = 0;
    
    // Hex format: [00-01] Header, [02-04] Voltage×100, [05] Status, [06-07] Battery
    if (!parse_hex_substring(2, 3, voltage_raw) || 
        !parse_hex_substring(5, 1, status_raw) || 
        !parse_hex_substring(6, 2, battery_raw)) {
      ESP_LOGW(TAG, "Failed parsing hex payload");
      return;
    }

    float volts = voltage_raw / 100.0f;
    int battery_pct = battery_raw;

    ESP_LOGD(TAG, "Parsed: voltage=%.2fV, status=%d, battery=%d%%", volts, status_raw, battery_pct);
    
    this->update_entities(volts, status_raw, battery_pct);
  } else {
    ESP_LOGW(TAG, "Hex too short to parse: len=%d", (int)hex.size());
  }
}

void BM2BLEComponent::update_entities(float voltage, int status, int battery) {
  // Update voltage sensor
  if (auto it = this->sensors_.find("voltage"); it != this->sensors_.end()) {
    it->second->publish_state(voltage);
  }

  // Update battery sensor
  if (auto it = this->sensors_.find("battery"); it != this->sensors_.end()) {
    it->second->publish_state((float)battery);
  }

  // Update status text sensor
  if (auto it = this->text_sensors_.find("status"); it != this->text_sensors_.end()) {
    std::string label = "unknown";
    switch (status) {
      case 0: label = "normal"; break;
      case 1: label = "weak"; break;
      case 2: label = "very weak"; break;
      case 4: label = "charging"; break;
      default: label = "unknown"; break;
    }
    it->second->publish_state(label);
  }

  // Update binary sensor: charging (status == 4)
  if (auto it = this->binary_sensors_.find("charging"); it != this->binary_sensors_.end()) {
    it->second->publish_state(status == 4);
  }

  // Update binary sensor: weak_battery (status == 1 or 2)
  if (auto it = this->binary_sensors_.find("weak_battery"); it != this->binary_sensors_.end()) {
    it->second->publish_state(status == 1 || status == 2);
  }
}

void BM2BLEComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  // We only care about notification events here
  if (event == ESP_GATTC_NOTIFY_EVT) {
    if (param->notify.handle == this->notify_handle_) {
      std::vector<uint8_t> data(param->notify.value, param->notify.value + param->notify.value_len);
      this->decrypt_and_handle(data);
    }
  }
}

}  // namespace bm2_ble
}  // namespace esphome
