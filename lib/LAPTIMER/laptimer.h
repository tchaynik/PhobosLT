#include <Arduino.h>
#include "RX5808.h"
#include "buzzer.h"
#include "config.h"
#include "kalman.h"
#include "led.h"

typedef enum {
    STOPPED,
    COUNTDOWN,  // Новий стан для обратного відліку
    WAITING,
    RUNNING
} laptimer_state_e;

#define LAPTIMER_LAP_HISTORY 10
#define LAPTIMER_RSSI_HISTORY 100

class LapTimer {
   public:
    void init(Config *config, RX5808 *rx5808, Buzzer *buzzer, Led *l);
    void start();
    void stop();
    void handleLapTimerUpdate(uint32_t currentTimeMs);
    uint8_t getRssi();
    uint32_t getLapTime();
    bool isLapAvailable();
    
    // Додаткові методи для OLED дисплея
    laptimer_state_e getState() { return state; }
    uint8_t getLapCount() { return lapCount; }
    
    // Колбеки для звукових подій на веб-сторінці
    void setCountdownBeepCallback(void (*callback)(int countNumber));
    void setRaceStartCallback(void (*callback)());
    void setLapCompleteCallback(void (*callback)(int lapNumber, uint32_t lapTime));
    void setRaceFinishCallback(void (*callback)());
    String getRaceStatus(); // Повертає статус для OLED

   private:
    laptimer_state_e state = STOPPED;
    RX5808 *rx;
    Config *conf;
    Buzzer *buz;
    Led *led;
    KalmanFilter filter;
    boolean lapCountWraparound;
    uint32_t raceStartTimeMs;
    uint32_t startTimeMs;
    uint8_t lapCount;
    uint8_t rssiCount;
    uint32_t lapTimes[LAPTIMER_LAP_HISTORY];
    uint8_t rssi[LAPTIMER_RSSI_HISTORY];

    uint8_t rssiPeak;
    uint32_t rssiPeakTimeMs;
    
    // Countdown змінні
    uint32_t countdownStartTime;
    uint32_t lastCountdownBeep;
    uint8_t countdownCounter;

    bool lapAvailable = false;
    
    // Колбеки для веб-подій
    void (*countdownBeepCallback)(int countNumber) = nullptr;
    void (*raceStartCallback)() = nullptr;
    void (*lapCompleteCallback)(int lapNumber, uint32_t lapTime) = nullptr;
    void (*raceFinishCallback)() = nullptr;

    void lapPeakCapture();
    bool lapPeakCaptured();
    void lapPeakReset();

    void startLap();
    void finishLap();
};
