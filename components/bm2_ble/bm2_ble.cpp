// bm2_ble.cpp
#include "bm2_ble.h"
#include "esphome.h"
#include <esp_gattc_api.h>
#include <vector>
#include <cstring>
#include <mbedtls/aes.h>

namespace esphome {
namespace bm2_ble {

static const char *TAG = "bm2_ble";

void BM2BLEComponent::setup() {
  ESP_LOGI(TAG, "BM2BLEComponent setup");
  // Register this component as a BLE node so we get GATTC events
  if (this->ble_client_ != nullptr) {
    this->ble_client_->register_ble_node(this);
  }
}

void BM2BLEComponent::loop() {
  uint32_t now = millis();
  if (!this->subscribed_ && this->ble_client_ != nullptr) {
    if (this->ble_client_->connected()) {
      this->ensure_subscription();
    }
  }
  (void)now;
}

void BM2BLEComponent::add_entity(const std::string &role, sensor::Sensor *s) {
  if (role == "voltage") {
    this->set_voltage_sensor(s);
  } else if (role == "battery") {
    this->set_battery_sensor(s);
  } else {
    ESP_LOGW(TAG, "Unknown sensor role '%s' (sensor)", role.c_str());
  }
}

void BM2BLEComponent::add_entity(const std::string &role, text_sensor::TextSensor *t) {
  if (role == "status") {
    this->set_status_text(t);
  } else {
    ESP_LOGW(TAG, "Unknown text_sensor role '%s'", role.c_str());
  }
}

void BM2BLEComponent::ensure_subscription() {
  if (this->subscribed_) return;
  if (!this->ble_client_->connected()) {
    ESP_LOGW(TAG, "BLE client not connected");
    return;
  }

  // The BM2 device uses service 0xFFF0 with characteristics FFF3 (write) and FFF4 (notify).
  auto svc = this->ble_client_->get_service(0xfff0);
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

  // Request notification registration from the BLE stack. BLEClientBase will handle
  // the result in its gattc_event_handler, but we still need to register here.
  esp_err_t err = esp_ble_gattc_register_for_notify(this->ble_client_->get_gattc_if(),
                                                   this->ble_client_->get_remote_bda(),
                                                   this->notify_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed: %d", err);
    // Do not return here - we may still get notifications depending on device
  }

  this->subscribed_ = true;
  ESP_LOGI(TAG, "Subscribed to notifications");
}

void BM2BLEComponent::send_command(const std::vector<uint8_t> &cmd) {
  auto svc = this->ble_client_->get_service(0xfff0);
  if (svc == nullptr) {
    ESP_LOGW(TAG, "Service 0xfff0 not found for send_command");
    return;
  }
  auto write_char = svc->get_characteristic(0xfff3);
  if (write_char == nullptr) {
    ESP_LOGW(TAG, "Write characteristic not found");
    return;
  }

  std::vector<uint8_t> in(cmd.begin(), cmd.end());
  size_t pad = (16 - (in.size() % 16)) % 16;
  if (pad) in.insert(in.end(), pad, 0);

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_enc(&ctx, AES_KEY, 128) != 0) {
    ESP_LOGE(TAG, "AES setkey_enc failed");
    mbedtls_aes_free(&ctx);
    return;
  }
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

  ESP_LOGI(TAG, "Decrypted HEX: %s", hex.c_str());
  this->handle_voltage_hex(hex);
}

void BM2BLEComponent::handle_voltage_hex(const std::string &hex) {
  if (hex.rfind("fefefe", 0) == 0) {
    ESP_LOGI(TAG, "Received fefefe diagnostic marker");
    return;
  }

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
    if (!parse_hex_substring(2, 3, voltage_raw) || !parse_hex_substring(5, 1, status_raw) || !parse_hex_substring(6, 2, battery_raw)) {
      ESP_LOGW(TAG, "Failed parsing hex payload");
      return;
    }

      float volts = voltage_raw / 100.0f;
      int battery_pct = battery_raw;

      if (this->voltage_sensor_)
        this->voltage_sensor_->publish_state(volts);
      if (this->battery_sensor_)
        this->battery_sensor_->publish_state((float)battery_pct);
      if (this->status_text_) {
        std::string label = "unknown";
        switch (status_raw) {
          case 0: label = "normal"; break;
          case 1: label = "weak"; break;
          case 2: label = "very weak"; break;
          case 4: label = "charging"; break;
          default: label = "unknown"; break;
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

bool BM2BLEComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  // We only care about notification events here
  if (event == ESP_GATTC_NOTIFY_EVT) {
    if (param->notify.handle == this->notify_handle_) {
      std::vector<uint8_t> data(param->notify.value, param->notify.value + param->notify.value_len);
      this->decrypt_and_handle(data);
      return true;
    }
  }
  return false;
}

}  // namespace bm2_ble
}  // namespace esphome
