#include "bm2_ble.h"
#include "esphome.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <mbedtls/aes.h>

#ifdef ARDUINO_ARCH_ESP32
#include "NimBLEDevice.h"
#endif

namespace esphome {
namespace bm2_ble {

using namespace std;

static const char *TAG = "bm2_ble_component";

void BM2BLEComponent::setup() {
  ESP_LOGI(TAG, "BM2BLEComponent setup mac=%s", mac_.c_str());
  // nothing else here; we'll connect on loop
}

void BM2BLEComponent::loop() {
  // try connect every 10 seconds if not connected
  unsigned long now = millis();
  if (!this->connected_ && (now - last_connect_attempt_ > 10000)) {
    last_connect_attempt_ = now;
#ifdef ARDUINO_ARCH_ESP32
    ESP_LOGI(TAG, "Attempting to start NimBLE and connect...");
    if (!NimBLEDevice::getInitialized()) {
      NimBLEDevice::init("esphome-bm2");
      NimBLEDevice::setSecurityAuth(false, false, false);
    }
    // parse MAC string AA:BB:CC:DD:EE:FF
    NimBLEAddress addr(this->mac_);
    NimBLEClient* pClient = NimBLEDevice::createClient();
    if (!pClient) {
      ESP_LOGE(TAG, "createClient failed");
      return;
    }
    ESP_LOGI(TAG, "Connecting to %s ...", this->mac_.c_str());
    if (!pClient->connect(addr)) {
      ESP_LOGW(TAG, "Failed to connect");
      NimBLEDevice::deleteClient(pClient);
      return;
    }
    ESP_LOGI(TAG, "Connected to device");
    this->connected_ = true;

    // discover services and characteristics
    NimBLERemoteService* svc = pClient->getService("0000fff0-0000-1000-8000-00805f9b34fb");
    if (!svc) {
      ESP_LOGW(TAG, "Service fff0 not found, trying generic discovery");
      svc = pClient->getServices().front();
    }
    NimBLERemoteCharacteristic* notifyChar = nullptr;
    NimBLERemoteCharacteristic* writeChar = nullptr;
    if (svc) {
      notifyChar = svc->getCharacteristic("0000fff4-0000-1000-8000-00805f9b34fb");
      writeChar  = svc->getCharacteristic("0000fff3-0000-1000-8000-00805f9b34fb");
    }
    if (!notifyChar) {
      ESP_LOGE(TAG, "Notify characteristic not found");
    } else {
      // subscribe with callback
      notifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* rc, NimBLECharacteristic::ValueType val) {
        // val is std::string or vector depending on lib; convert to bytes
        std::vector<uint8_t> bytes(val.begin(), val.end());
        this->decrypt_and_handle(bytes);
      });
      ESP_LOGI(TAG, "Subscribed to notify characteristic");
    }
#else
    ESP_LOGW(TAG, "BLE not supported on this build (NimBLE missing)");
#endif
  }
}

void BM2BLEComponent::send_command(const std::vector<uint8_t> &cmd) {
#ifdef ARDUINO_ARCH_ESP32
  // Find client and write characteristic - this simplified example assumes a single client exists
  NimBLEDevice::getClientList().front()->getService("0000fff0-0000-1000-8000-00805f9b34fb")
      ->getCharacteristic("0000fff3-0000-1000-8000-00805f9b34fb")
      ->writeValue((uint8_t*)cmd.data(), cmd.size(), true);
#endif
}

// AES-CBC decrypt with zero IV, zero padding expected
void BM2BLEComponent::decrypt_and_handle(const std::vector<uint8_t>& data) {
  // ensure length multiple of 16 by padding zeros if necessary
  size_t len = data.size();
  size_t pad = (16 - (len % 16)) % 16;
  std::vector<uint8_t> in(data);
  if (pad) in.insert(in.end(), pad, 0);

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_dec(&ctx, AES_KEY, 128);

  std::vector<uint8_t> out(in.size(), 0);
  uint8_t iv[16] = {0};
  // mbedtls AES-CBC decrypt
  int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, in.size(), iv, in.data(), out.data());
  if (rc != 0) {
    ESP_LOGE(TAG, "AES decrypt failed: %d", rc);
    mbedtls_aes_free(&ctx);
    return;
  }
  mbedtls_aes_free(&ctx);

  // convert to hex lowercase string
  std::string hex = "";
  for (size_t i = 0; i < out.size(); i++) {
    char buf[3];
    sprintf(buf, "%02x", out[i]);
    hex += std::string(buf);
  }
  // trim trailing zeros (from padding)
  while (!hex.empty() && hex.size() >= 2 && hex.substr(hex.size()-2) == "00") {
    hex.erase(hex.size()-2);
  }
  ESP_LOGI(TAG, "Decrypted HEX: %s", hex.c_str());
  this->handle_voltage_hex(hex);
}

void BM2BLEComponent::handle_voltage_hex(const std::string &hex) {
  // If starts with fefefe -> ignore diagnostic marker
  if (hex.rfind("fefefe", 0) == 0) {
    ESP_LOGI(TAG, "Received diagnostic marker fefefe");
    return;
  }
  if (hex.size() >= 16) {
    // match: substring(2,5), etc.
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

std::string BM2BLEComponent::bytes_to_hex(const uint8_t *data, size_t len) {
  std::string hex="";
  for (size_t i=0;i<len;i++) {
    char buf[3];
    sprintf(buf, "%02x", data[i]);
    hex += std::string(buf);
  }
  return hex;
}

void BM2BLEComponent::ensure_connected() {
  // placeholder for explicit connect logic; used by higher-level code if needed
}

void BM2BLEComponent::disconnect_ble() {
  // placeholder
}

}  // namespace bm2_ble
}  // namespace esphome
