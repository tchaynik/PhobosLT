#include "oled.h"
#include "../CONFIG/config.h"

void OledDisplay::init(int sda_pin, int scl_pin) {
    Wire.begin(sda_pin, scl_pin);
    
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    
    if(!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        initialized = false;
        return;
    }
    
    initialized = true;
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    
    // Показуємо splash screen (адаптовано для маленького екрану)
    display->setTextSize(1);
    centerText("PhobosLT", 5);
    display->setTextSize(1);
    centerText("Init...", 20);
    display->display();
    delay(2000);
}

void OledDisplay::displayWiFiInfo(const String& ssid, const String& ip, wifi_mode_t mode, const String& channel_info, bool blinkBand, const String& raceStatus, bool timerActive, float batteryVoltage) {
    if (!initialized) return;
    
    display->clearDisplay();
    display->setTextSize(1);
    
    // Якщо таймер активний - інвертуємо дисплей або робимо мигання
    bool shouldInvert = false;
    if (timerActive) {
        // Мигання фону коли таймер активний
        shouldInvert = shouldShowBlinkingText(millis());
    }
    
    // Встановлюємо колір тексту залежно від інверсії
    display->setTextColor(shouldInvert ? SSD1306_BLACK : SSD1306_WHITE);
    
    // Якщо потрібна інверсія - заливаємо фон білим
    if (shouldInvert) {
        display->fillScreen(SSD1306_WHITE);
    }
    
    // Рядок 1: Канал + назва мережі в форматі "R1 PhobosAP"
    display->setCursor(0, 0);
    if (channel_info.length() > 0) {
        // Якщо режим блимання бенду активний
        if (blinkBand && shouldShowBlinkingText(millis())) {
            // Показуємо тільки номер каналу без букви бенду
            String channelOnly = channel_info.substring(1); // Прибираємо першу букву
            display->print("_" + channelOnly + " ");
        } else if (blinkBand) {
            // Не показуємо букву бенду (блимає)
            String channelOnly = channel_info.substring(1);
            display->print("_" + channelOnly + " ");
        } else {
            // Звичайний режим - показуємо канал
            // Якщо таймер активний - мигаємо каналом
            if (timerActive && shouldShowBlinkingText(millis())) {
                display->print(">> " + channel_info + " <<");
            } else {
                display->print(channel_info + " ");
            }
        }
    }
    
    // Додаємо назву мережі (скорочену якщо таймер неактивний)
    if (!timerActive || !shouldInvert) {
        String network_name = ssid;
        int maxNameLength = 72 - (channel_info.length() + 5) * 6; // Враховуємо ширину каналу
        if (network_name.length() * 6 > maxNameLength) {
            network_name = network_name.substring(0, maxNameLength / 6);
        }
        display->print(network_name);
    }
    
    // Рядок 2: IP адреса
    display->setCursor(0, 10);
    display->print(ip);
    
    // Рядок 3: Статус гонки
    display->setCursor(0, 20);
    if (raceStatus.length() > 0) {
        // Якщо таймер активний - підкреслюємо статус
        if (timerActive) {
            display->print("* " + raceStatus + " *");
        } else {
            display->print(raceStatus);
        }
    } else if (channel_info.length() > 0) {
        // Показуємо частоту якщо немає статусу гонки
        int frequency = getChannelFrequency(channel_info);
        display->print(String(frequency) + "MHz");
    }
    
    // Рядок 4: Індикатор батареї (якщо напруга передана)
    if (batteryVoltage > 0.0) {
        // Малюємо індикатор батареї в правому нижньому куті
        drawBatteryIndicator(batteryVoltage, SCREEN_WIDTH - 20, 30);
        
        // Показуємо напругу цифрами
        display->setCursor(0, 30);
        display->print(String(batteryVoltage, 1) + "V");
    }
    
    display->display();
}

bool OledDisplay::shouldShowBlinkingText(uint32_t currentTime) {
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
    }
    return blinkState;
}

void OledDisplay::displayMessage(const String& line1, const String& line2, const String& line3, const String& line4) {
    if (!initialized) return;
    
    display->clearDisplay();
    display->setTextSize(1);
    
    // Для 0.42" екрану використовуємо компактніше розташування
    if (line1.length() > 0) {
        display->setCursor(0, 0);
        display->print(line1);
    }
    if (line2.length() > 0) {
        display->setCursor(0, 10);
        display->print(line2);
    }
    if (line3.length() > 0) {
        display->setCursor(0, 20);
        display->print(line3);
    }
    if (line4.length() > 0) {
        display->setCursor(0, 30);
        display->print(line4);
    }
    
    display->display();
}

void OledDisplay::clear() {
    if (!initialized) return;
    display->clearDisplay();
    display->display();
}

void OledDisplay::update() {
    if (!initialized) return;
    display->display();
}

void OledDisplay::centerText(const String& text, int y) {
    if (!initialized) return;
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    display->setCursor(x, y);
    display->print(text);
}

void OledDisplay::drawBatteryIndicator(float voltage, int x, int y) {
    if (!initialized) return;
    
    // Розмір батареї для маленького екрану
    const int batteryWidth = 16;
    const int batteryHeight = 8;
    const int tipWidth = 2;
    const int tipHeight = 4;
    
    // Обчислюємо рівень заряду (3.0V - 4.2V)
    float minVoltage = 3.0;
    float maxVoltage = 4.2;
    int chargeLevel = (int)((voltage - minVoltage) / (maxVoltage - minVoltage) * (batteryWidth - 2));
    if (chargeLevel < 0) chargeLevel = 0;
    if (chargeLevel > batteryWidth - 2) chargeLevel = batteryWidth - 2;
    
    // Малюємо корпус батареї
    display->drawRect(x, y, batteryWidth, batteryHeight, SSD1306_WHITE);
    
    // Малюємо "носик" батареї
    int tipX = x + batteryWidth;
    int tipY = y + (batteryHeight - tipHeight) / 2;
    display->fillRect(tipX, tipY, tipWidth, tipHeight, SSD1306_WHITE);
    
    // Малюємо рівень заряду
    if (chargeLevel > 0) {
        // Колір заливки залежно від рівня
        bool shouldFill = true;
        
        // Якщо заряд низький - блимаємо
        if (voltage < 3.3 && shouldShowBlinkingText(millis())) {
            shouldFill = false;
        }
        
        if (shouldFill) {
            display->fillRect(x + 1, y + 1, chargeLevel, batteryHeight - 2, SSD1306_WHITE);
        }
    }
}