#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// Параметри для 0.42" OLED на ESP32C3
#define SCREEN_WIDTH 72
#define SCREEN_HEIGHT 40
#define SCREEN_OFFSET_X 13
#define SCREEN_OFFSET_Y 14
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

class OledDisplay {
   public:
    void init(int sda_pin, int scl_pin);
    void displayWiFiInfo(const String& ssid, const String& ip, wifi_mode_t mode, const String& channel_info = "", bool blinkBand = false, const String& raceStatus = "", bool timerActive = false);
    void displayMessage(const String& line1, const String& line2 = "", const String& line3 = "", const String& line4 = "");
    void clear();
    void update();
    bool isInitialized() { return initialized; }

   private:
    Adafruit_SSD1306* display;
    bool initialized = false;
    bool blinkState = false;
    uint32_t lastBlinkTime = 0;
    static const uint32_t BLINK_INTERVAL = 500; // 500мс
    void centerText(const String& text, int y);
    bool shouldShowBlinkingText(uint32_t currentTime);
};