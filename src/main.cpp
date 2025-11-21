#include "debug.h"
#include "webserver.h"
#include "oled.h"
#include "buttons.h"
#include <ElegantOTA.h>

static RX5808 rx(PIN_RX5808_RSSI, PIN_RX5808_DATA, PIN_RX5808_SELECT, PIN_RX5808_CLOCK);
static Config config;
static Webserver ws;
static Buzzer buzzer;
static Led led;
static LapTimer timer;
static BatteryMonitor monitor;
static OledDisplay oled;
static ButtonHandler buttons;

static TaskHandle_t xTimerTask = NULL;

static void parallelTask(void *pvArgs) {
    uint32_t lastOledUpdate = 0;
    const uint32_t OLED_UPDATE_INTERVAL = 1000; // Оновлюємо OLED кожну секунду
    
    for (;;) {
        uint32_t currentTimeMs = millis();
        buzzer.handleBuzzer(currentTimeMs);
        led.handleLed(currentTimeMs);
        ws.handleWebUpdate(currentTimeMs);
        config.handleEeprom(currentTimeMs);
        rx.handleFrequencyChange(currentTimeMs, config.getFrequency());
        monitor.checkBatteryState(currentTimeMs, config.getAlarmThreshold());
        buttons.handleButtons(currentTimeMs);
        
#ifdef ESP32C3
        // Частіше оновлення OLED для блимання в режимі бенду
        uint32_t updateInterval = buttons.isBandModeActive() ? 100 : OLED_UPDATE_INTERVAL;
        if ((currentTimeMs - lastOledUpdate) > updateInterval) {
            ws.updateOledDisplay();
            lastOledUpdate = currentTimeMs;
        }
#endif
        buzzer.handleBuzzer(currentTimeMs);
        led.handleLed(currentTimeMs);
    }
}

static void initParallelTask() {
    disableCore0WDT();
    xTaskCreatePinnedToCore(parallelTask, "parallelTask", 3000, NULL, 0, &xTimerTask, 0);
}

// Колбек для зміни частоти через кнопки
static void onFrequencyChanged(uint16_t frequency) {
    config.setFrequency(frequency);
    buzzer.beep(100); // Короткий сигнал при зміні каналу
    led.blink(100);   // Миготіння LED
}

// Колбек для зміни каналу
static void onChannelChanged(uint8_t band, uint8_t channel) {
    DEBUG("Channel changed: Band %s, Channel %d\n", FPVChannels::getBandName(band), channel + 1);
    
    // Оновлюємо OLED з інформацією про канал через webserver
#ifdef ESP32C3
    // Затримка для того щоб дати час webserver оновити екран
    delay(100);
#endif
}

// Колбек для режиму зміни бенду
static void onBandModeChanged(bool bandModeActive) {
    if (bandModeActive) {
        DEBUG("Band selection mode activated\n");
        buzzer.beep(200); // Довгий сигнал входу в режим бенду
    } else {
        DEBUG("Band selection mode deactivated\n");
        buzzer.beep(50);  // Короткий сигнал виходу з режиму
    }
}

// Колбек для керування таймером через кнопку
static void onTimerControl(bool startTimer) {
    if (startTimer) {
        DEBUG("Timer start requested via button\n");
        timer.start();  // Запускаємо countdown
        buzzer.tone(800, 250); // Звуковий сигнал старту
        buttons.setTimerActive(true); // Блокуємо кнопки каналів
    } else {
        DEBUG("Timer stop requested via button\n");
        timer.stop();   // Зупиняємо таймер
        buzzer.tone(400, 500); // Звуковий сигнал зупинки (низький тон)
        buttons.setTimerActive(false); // Розблокуємо кнопки каналів
    }
}

void setup() {
    DEBUG_INIT;
    
    // Ініціалізуємо OLED одним з перших
#ifdef ESP32C3
    oled.init(PIN_OLED_SDA, PIN_OLED_SCL);
#endif

#ifdef DEBUG_OUT
    DEBUG("PhobosLT ESP32C3 - DEBUG MODE ENABLED\n");
    DEBUG("Serial baud: %d\n", SERIAL_BAUD);
    DEBUG("Free heap: %d bytes\n", ESP.getFreeHeap());
#endif
    
    config.init();
    rx.init();
    buzzer.init(PIN_BUZZER, BUZZER_INVERTED);
    led.init(PIN_LED, false);
    timer.init(&config, &rx, &buzzer, &led);
    monitor.init(PIN_VBAT, VBAT_SCALE, VBAT_ADD, &buzzer, &led);
    
    // Ініціалізуємо кнопки перед webserver
#ifdef ESP32C3
    buttons.init();
    buttons.setFrequencyChangeCallback(onFrequencyChanged);
    buttons.setChannelChangeCallback(onChannelChanged);
    buttons.setBandModeCallback(onBandModeChanged);
    buttons.setTimerControlCallback(onTimerControl);
    
    // Встановлюємо поточну частоту з конфігурації
    buttons.setCurrentFrequency(config.getFrequency());
#endif
    
    // Ініціалізуємо webserver з кнопками
    ws.init(&config, &timer, &monitor, &buzzer, &led, &oled, &buttons);
    
    // Встановлюємо колбеки для відправки звукових подій на веб-сторінку
    timer.setCountdownBeepCallback([](int countNumber) {
        ws.sendCountdownBeepEvent(countNumber);
    });
    timer.setRaceStartCallback([]() {
        ws.sendRaceStartEvent();
    });
    timer.setLapCompleteCallback([](int lapNumber, uint32_t lapTime) {
        ws.sendLapCompleteEvent(lapNumber, lapTime);
    });
    timer.setRaceFinishCallback([]() {
        ws.sendRaceFinishEvent();
    });
    
    led.on(400);
    buzzer.beep(200);
    initParallelTask();
    
    // Виводимо стартову інформацію
    const char* deviceModeNames[] = {"Standalone", "Master", "Slave"};
    uint8_t mode = config.getDeviceMode();
    
#ifdef DEBUG_OUT
    DEBUG("=== PhobosLT Ready ===\n");
    DEBUG("Device Mode: %s\n", deviceModeNames[mode]);
    DEBUG("Node ID: %s\n", config.getNodeId());
    if (mode == MODE_SLAVE) {
        DEBUG("Master IP: %s\n", config.getMasterIP());
        DEBUG("Channel: %d\n", config.getNodeChannel());
    }
    DEBUG("MAC Address: %s\n", WiFi.macAddress().c_str());
    DEBUG("Free heap: %d bytes\n", ESP.getFreeHeap());
    DEBUG("=====================\n");
#endif
}

void loop() {
    uint32_t currentTimeMs = millis();
    timer.handleLapTimerUpdate(currentTimeMs);
    
    // Обробляємо кнопки
#ifdef ESP32C3
    buttons.handleButtons(currentTimeMs);
#endif
    
    // Оновлюємо OLED кожні 100мс
    static uint32_t lastOledUpdate = 0;
    if (currentTimeMs - lastOledUpdate > 100) {
        ws.updateOledDisplay();
        lastOledUpdate = currentTimeMs;
    }
    
    ElegantOTA.loop();
}
