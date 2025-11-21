#include "buttons.h"
#include <Arduino.h>

// Частоти FPV каналів в MHz
const uint16_t FPVChannels::BAND_A[8] = {5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725}; // Boscam A
const uint16_t FPVChannels::BAND_B[8] = {5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866}; // Boscam B
const uint16_t FPVChannels::BAND_E[8] = {5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945}; // Boscam E
const uint16_t FPVChannels::BAND_F[8] = {5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880}; // Fatshark
const uint16_t FPVChannels::BAND_R[8] = {5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917}; // Raceband

const char* FPVChannels::getBandName(uint8_t band) {
    switch(band) {
        case 0: return "Band A";
        case 1: return "Band B";
        case 2: return "Band E";
        case 3: return "Band F";
        case 4: return "Band R";
        default: return "Unknown";
    }
}

const char* FPVChannels::getBandShortName(uint8_t band) {
    switch(band) {
        case 0: return "A";
        case 1: return "B";
        case 2: return "E";
        case 3: return "F";
        case 4: return "R";
        default: return "?";
    }
}

const uint16_t* FPVChannels::getBandChannels(uint8_t band) {
    switch(band) {
        case 0: return BAND_A;
        case 1: return BAND_B;
        case 2: return BAND_E;
        case 3: return BAND_F;
        case 4: return BAND_R;
        default: return BAND_R;
    }
}

void ButtonHandler::init() {
    pinMode(BUTTON_BOOT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_CHANNEL_PIN, INPUT_PULLUP);
    lastBootButtonState = digitalRead(BUTTON_BOOT_PIN);
    lastChannelButtonState = digitalRead(BUTTON_CHANNEL_PIN);
}

void ButtonHandler::handleButtons(uint32_t currentTimeMs) {
    // Обробка таймауту режиму бенду
    processBandModeTimeout(currentTimeMs);
    
    // Обробка кнопки BOOT
    bool currentBootState = digitalRead(BUTTON_BOOT_PIN);
    
    // Перевірка debounce
    if (currentBootState != lastBootButtonState) {
        lastBootButtonTime = currentTimeMs;
        if (currentBootState == LOW) {
            // Початок натискання
            buttonPressStartTime = currentTimeMs;
        } else if (currentBootState == HIGH && lastBootButtonState == LOW) {
            // Кінець натискання - перевіряємо тривалість
            uint32_t pressDuration = currentTimeMs - buttonPressStartTime;
            
            if (pressDuration >= veryLongPressTime) {
                // Дуже довгий натиск (3+ секунди) - керування таймером
                if (timerControlCallback) {
                    timerControlCallback(!timerActive); // Інвертуємо стан таймера
                }
            } else if (pressDuration >= longPressTime && !timerActive) {
                // Довгий натиск (800мс+) - режим бенду (тільки якщо таймер неактивний)
                if (!bandModeActive) {
                    bandModeActive = true;
                    bandModeStartTime = currentTimeMs;
                    if (bandModeCallback) {
                        bandModeCallback(true);
                    }
                }
            } else if (pressDuration > buttonDebounceTime && !timerActive) {
                // Короткий натиск (тільки якщо таймер неактивний)
                if (bandModeActive) {
                    // У режимі бенду - змінюємо бенд
                    nextBand();
                    bandModeStartTime = currentTimeMs; // Перезапускаємо таймер
                } else {
                    // Звичайний режим - змінюємо канал
                    nextChannel();
                }
            }
        }
    }
    
    lastBootButtonState = currentBootState;
}

void ButtonHandler::processBandModeTimeout(uint32_t currentTimeMs) {
    if (bandModeActive && (currentTimeMs - bandModeStartTime) >= bandModeTimeout) {
        bandModeActive = false;
        if (bandModeCallback) {
            bandModeCallback(false);
        }
    }
}

void ButtonHandler::setBandModeCallback(void (*callback)(bool bandModeActive)) {
    bandModeCallback = callback;
}

void ButtonHandler::setTimerControlCallback(void (*callback)(bool startTimer)) {
    timerControlCallback = callback;
}

void ButtonHandler::nextChannel() {
    currentChannel++;
    if (currentChannel >= 8) {
        currentChannel = 0; // Залишаємося в тому ж бенді, просто повертаємось до каналу 1
    }
    updateFrequency();
}

void ButtonHandler::nextBand() {
    currentBand++;
    if (currentBand >= 5) {
        currentBand = 0;
    }
}

void ButtonHandler::updateFrequency() {
    uint16_t newFreq = getCurrentFrequency();
    
    if (channelChangeCallback) {
        channelChangeCallback(currentBand, currentChannel);
    }
    
    if (frequencyChangeCallback) {
        frequencyChangeCallback(newFreq);
    }
}

uint16_t ButtonHandler::getCurrentFrequency() {
    const uint16_t* channels = FPVChannels::getBandChannels(currentBand);
    return channels[currentChannel];
}

String ButtonHandler::getChannelInfo() {
    String info = String(FPVChannels::getBandShortName(currentBand));
    info += String(currentChannel + 1);  // Канали 1-8 замість 0-7
    return info;
}

void ButtonHandler::setCurrentChannel(uint8_t band, uint8_t channel) {
    if (band < 5 && channel < 8) {
        currentBand = band;
        currentChannel = channel;
    }
}

void ButtonHandler::setCurrentFrequency(uint16_t frequency) {
    // Знаходимо найближчу частоту
    for (uint8_t band = 0; band < 5; band++) {
        const uint16_t* channels = FPVChannels::getBandChannels(band);
        for (uint8_t ch = 0; ch < 8; ch++) {
            if (channels[ch] == frequency) {
                currentBand = band;
                currentChannel = ch;
                return;
            }
        }
    }
}

void ButtonHandler::setChannelChangeCallback(void (*callback)(uint8_t band, uint8_t channel)) {
    channelChangeCallback = callback;
}

void ButtonHandler::setFrequencyChangeCallback(void (*callback)(uint16_t frequency)) {
    frequencyChangeCallback = callback;
}