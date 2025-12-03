#include "bm2_ble.h"
#include "esphome.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <mbedtls/aes.h>

namespace esphome {
namespace bm2_ble {

static const char *TAG = "bm2_ble";

void BM2BLEComponent::setup() {
  ESP_LOGI(TAG, "BM2BLEComponent setup");
}

void BM2BLEComponent::loop() {
  uint32_t now = millis();
  if (!this->subscribed_ && this->ble_client_ != nullptr) {
    if (this->ble_client_->is_connected()) {
      this->ensure_subscription();
    }
  }
  (void)now;
}

void BM2BLEComponent::ensure_subscription() {
  if (this->subscribed_) return;
  if (!this->ble_client_->is_connected()) {
    ESP_LOGW(TAG, "BLE client not connected");
    return;
  }

  // Attempt to find notify and write characteristics
  auto notify_char = this->ble_client_->get_characteristic_by_uuid("0000fff4-0000-1000-8000-00805f9b34fb");
  auto write_char  = this->ble_client_->get_characteristic_by_uuid("0000fff3-0000-1000-8000-00805f9b34fb");
  if (notify_char == nullptr) {
    ESP_LOGW(TAG, "Notify characteristic not found");
    return;
  }

  // Register callback to receive raw bytes
  this->ble_client_->add_on_notify_callback(notify_char->get_handle(), [this](const std::vector<uint8_t> &data) {
    this->decrypt_and_handle(data);
  });

  this->subscribed_ = true;
  ESP_LOGI(TAG, "Subscribed to notifications");
}

void BM2BLEComponent::send_command(const std::vector<uint8_t> &cmd) {
  auto write_char  = this->ble_client_->get_characteristic_by_uuid("0000fff3-0000-1000-8000-00805f9b34fb");
  if (write_char == nullptr) {
    ESP_LOGW(TAG, "Write characteristic not found");
    return;
  }
  std::vector<uint8_t> in(cmd.begin(), cmd.end());
  size_t pad = (16 - (in.size() % 16)) % 16;
  if (pad) in.insert(in.end(), pad, 0);
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, AES_KEY, 128);
  std::vector<uint8_t> out(in.size(), 0);
  uint8_t iv[16] = {0};
  int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, in.size(), iv, in.data(), out.data());
  mbedtls_aes_free(&ctx);
  if (rc != 0) {
    ESP_LOGE(TAG, "AES encrypt failed: %d", rc);
    return;
  }
  write_char->write_value(out.data(), out.size(), true);
}

void BM2BLEComponent::decrypt_and_handle(const std::vector<uint8_t>& data) {
  std::vector<uint8_t> in(data.begin(), data.end());
  size_t pad = (16 - (in.size() % 16)) % 16;
  if (pad) in.insert(in.end(), pad, 0);
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_dec(&ctx, AES_KEY, 128);
  std::vector<uint8_t> out(in.size(), 0);
  uint8_t iv[16] = {0};
  int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, in.size(), iv, in.data(), out.data());
  mbedtls_aes_free(&ctx);
  if (rc != 0) {
    ESP_LOGE(TAG, "AES decrypt failed: %d", rc);
    return;
  }
  std::string hex;
  hex.reserve(out.size()*2);
  for (size_t i=0;i<out.size();++i) {
    char buf[3];
    sprintf(buf, "%02x", out[i]);
    hex += buf;
  }
  while (hex.size() >= 2 && hex.substr(hex.size()-2) == "00") hex.erase(hex.size()-2);
  ESP_LOGI(TAG, "Decrypted HEX: %s", hex.c_str());
  this->handle_voltage_hex(hex);
}

void BM2BLEComponent::handle_voltage_hex(const std::string &hex) {
  if (hex.rfind("fefefe", 0) == 0) {
    ESP_LOGI(TAG, "Received fefefe diagnostic marker");
    return;
  }
  if (hex.size() >= 16) {
    try {
      auto sub = [&](size_t a, size_t b)->int {
        std::string s = hex.substr(a, b-a);
        return std::stoi(s, nullptr, 16);
      };
      int voltage_raw = sub(2,5);
      int status_raw = sub(5,6);
      int battery_raw = sub(6,8);
      float volts = voltage_raw / 100.0f;
      int battery_pct = battery_raw;
      if (this->voltage_sensor_) this->voltage_sensor_->publish_state(volts);
      if (this->battery_sensor_) this->battery_sensor_->publish_state((float)battery_pct);
      if (this->status_text_) {
        std::string label = "unknown";
        switch (status_raw) {
          case 0: label = "normal"; break;
          case 1: label = "weak"; break;
          case 2: label = "very weak"; break;
          case 4: label = "charging"; break;
          default: label = "unknown";
        }
        this->status_text_->publish_state(label);
      }
      ESP_LOGI(TAG, "Parsed volts=%.2f status=%d battery=%d", volts, status_raw, battery_pct);
    } catch (...) {
      ESP_LOGW(TAG, "Failed parsing hex payload");
    }
  } else {
    ESP_LOGW(TAG, "Hex too short to parse: len=%d", (int)hex.size());
  }
}
}  # namespace bm2_ble
}  # namespace esphome
