#pragma once

#include <stdint.h>
#include <Arduino.h>

// Піни кнопок на ESP32C3
#define BUTTON_BOOT_PIN 9    // Кнопка BOOT
#define BUTTON_RST_PIN  -1   // RST кнопка (спеціальна обробка)

// Стандартні частоти FPV каналів (MHz)
class FPVChannels {
public:
    // Band A (Boscam A)
    static const uint16_t BAND_A[8];
    // Band B (Boscam B)  
    static const uint16_t BAND_B[8];
    // Band E (Boscam E/DJI)
    static const uint16_t BAND_E[8];
    // Band F (Fatshark/Immersion)
    static const uint16_t BAND_F[8];
    // Band R (Raceband)
    static const uint16_t BAND_R[8];
    
    static const char* getBandName(uint8_t band);
    static const char* getBandShortName(uint8_t band);  // Коротка назва (A, B, E, F, R)
    static const uint16_t* getBandChannels(uint8_t band);
    static uint8_t getBandCount() { return 5; }
    static uint8_t getChannelCount() { return 8; }
};

class ButtonHandler {
public:
    void init();
    void handleButtons(uint32_t currentTimeMs);
    
    // Колбеки для зміни каналів
    void setChannelChangeCallback(void (*callback)(uint8_t band, uint8_t channel));
    void setFrequencyChangeCallback(void (*callback)(uint16_t frequency));
    void setBandModeCallback(void (*callback)(bool bandModeActive)); // Новий колбек для режиму бенду
    void setTimerControlCallback(void (*callback)(bool startTimer)); // Колбек для керування таймером
    
    // Поточний стан
    uint8_t getCurrentBand() { return currentBand; }
    uint8_t getCurrentChannel() { return currentChannel; }
    uint16_t getCurrentFrequency();
    String getChannelInfo();  // Повертає коротку інформацію типу "R1"
    bool isBandModeActive() { return bandModeActive; } // Перевірка режиму бенду
    
    // Методи для встановлення поточного каналу
    void setCurrentChannel(uint8_t band, uint8_t channel);
    void setCurrentFrequency(uint16_t frequency);
    
    // Блокування кнопок під час роботи таймера
    void setTimerActive(bool active) { timerActive = active; }
    
private:
    uint8_t currentBand = 4;    // Raceband за замовчуванням
    uint8_t currentChannel = 0; // Канал 1
    
    bool bootButtonPressed = false;
    bool lastBootButtonState = false;
    uint32_t lastBootButtonTime = 0;
    uint32_t buttonPressStartTime = 0;
    uint32_t buttonDebounceTime = 50; // 50ms debounce
    uint32_t longPressTime = 800;     // 800ms для довгого натискання (зміна бенду)
    uint32_t veryLongPressTime = 3000; // 3000ms для дуже довгого натискання (таймер)
    
    // Режим зміни бенду
    bool bandModeActive = false;
    uint32_t bandModeStartTime = 0;
    uint32_t bandModeTimeout = 1000;  // 1 секунда таймаут
    
    // Блокування кнопок під час роботи таймера
    bool timerActive = false;
    
    void (*channelChangeCallback)(uint8_t band, uint8_t channel) = nullptr;
    void (*frequencyChangeCallback)(uint16_t frequency) = nullptr;
    void (*bandModeCallback)(bool bandModeActive) = nullptr;
    void (*timerControlCallback)(bool startTimer) = nullptr;
    
    void nextChannel();
    void nextBand();
    void updateFrequency();
    void processBandModeTimeout(uint32_t currentTimeMs);
};