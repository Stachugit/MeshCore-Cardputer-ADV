#pragma once

#include <Wire.h>
#include <Arduino.h>
#include <M5Cardputer.h>
#include "helpers/ESP32Board.h"

#define PIN_VBAT_READ 10
#define BATTERY_SAMPLES 8

class M5CardputerBoard : public ESP32Board {
public:
  void begin() {
    // Initialize M5Cardputer hardware with keyboard enabled
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = false;  // No IMU on Cardputer-Adv
    cfg.internal_rtc = true;
    cfg.internal_spk = true;
    cfg.internal_mic = true;
    M5Cardputer.begin(cfg, true);  // true = enable keyboard

    // Call parent class initialization for standard ESP32 setup
    ESP32Board::begin();

    Serial.println("M5Stack Cardputer-Adv initialized");
    Serial.print("Battery voltage: ");
    Serial.print(getBattMilliVolts());
    Serial.println(" mV");
  }

  uint16_t getBattMilliVolts() override {
    #ifdef PIN_VBAT_READ
      analogReadResolution(12);
      uint32_t raw = 0;
      
      for (int i = 0; i < BATTERY_SAMPLES; i++) {
        raw += analogReadMilliVolts(PIN_VBAT_READ);
        delay(1);
      }
      
      raw = raw / BATTERY_SAMPLES;
      // Cardputer has a voltage divider (2:1)
      return (2 * raw);
    #else
      return 0;
    #endif
  }

  const char* getManufacturerName() const override {
    return "M5Stack Cardputer-Adv";
  }

  void enterDeepSleep(uint32_t secs, int pin_wake_btn) {
    // Configure deep sleep with LoRa interrupt wake
    esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);
    
    if (pin_wake_btn >= 0) {
      esp_sleep_enable_ext1_wakeup((1ULL << pin_wake_btn) | (1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);
    }
    
    if (secs > 0) {
      esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
    }
    
    Serial.println("Entering deep sleep...");
    Serial.flush();
    esp_deep_sleep_start();
  }
};
