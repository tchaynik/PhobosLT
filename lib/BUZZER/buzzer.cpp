#include "buzzer.h"

void Buzzer::init(uint8_t pin, bool inverted) {
    pinMode(pin, OUTPUT);
    initialState = inverted ? HIGH : LOW;
    buzzerPin = pin;
    buzzerState = BUZZER_IDLE;
    digitalWrite(buzzerPin, initialState);
}

void Buzzer::beep(uint32_t timeMs) {
    beepTimeMs = timeMs;
    buzzerState = BUZZER_BEEPING;
    startTimeMs = millis();
    digitalWrite(buzzerPin, !initialState);
}

void Buzzer::tone(uint32_t frequency, uint32_t timeMs) {
    beepTimeMs = timeMs;
    buzzerState = BUZZER_TONE;
    startTimeMs = millis();
    currentFrequency = frequency;
    
    // Використовуємо ledcWrite для ESP32
    ledcSetup(0, frequency, 8);  // Канал 0, частота, 8-біт роздільність
    ledcAttachPin(buzzerPin, 0);
    ledcWrite(0, 127);  // 50% duty cycle
}

void Buzzer::noTone() {
    ledcWrite(0, 0);  // Вимкнути PWM
    digitalWrite(buzzerPin, initialState);
    buzzerState = BUZZER_IDLE;
}

void Buzzer::handleBuzzer(uint32_t currentTimeMs) {
    switch (buzzerState) {
        case BUZZER_IDLE:
            break;
        case BUZZER_BEEPING:
            if (currentTimeMs < startTimeMs) {
                break;  // updated from different core
            }
            if ((currentTimeMs - startTimeMs) > beepTimeMs) {
                digitalWrite(buzzerPin, initialState);
                buzzerState = BUZZER_IDLE;
            }
            break;
        case BUZZER_TONE:
            if (currentTimeMs < startTimeMs) {
                break;  // updated from different core
            }
            if ((currentTimeMs - startTimeMs) > beepTimeMs) {
                noTone();
            }
            break;
        default:
            break;
    }
}
