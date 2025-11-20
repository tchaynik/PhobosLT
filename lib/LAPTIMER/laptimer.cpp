#include "laptimer.h"
#include <Arduino.h>
#include "debug.h"

const uint16_t rssi_filter_q = 2000;  //  0.01 - 655.36
const uint16_t rssi_filter_r = 40;    // 0.0001 - 65.536

void LapTimer::init(Config *config, RX5808 *rx5808, Buzzer *buzzer, Led *l) {
    conf = config;
    rx = rx5808;
    buz = buzzer;
    led = l;

    filter.setMeasurementNoise(rssi_filter_q * 0.01f);
    filter.setProcessNoise(rssi_filter_r * 0.0001f);

    stop();
    memset(rssi, 0, sizeof(rssi));
}

void LapTimer::start() {
    DEBUG("LapTimer countdown started\n");
    countdownStartTime = millis();
    lastCountdownBeep = 0;
    countdownCounter = 3; // 3 біпи (3, 2, 1)
    state = COUNTDOWN;
    
    // Перший біп одразу - "3"
    buz->tone(500, 250);  // 500Hz, 250мс - countdown біп
    led->blink(250);
    
    // Відправляємо подію на веб-сторінку
    if (countdownBeepCallback) {
        countdownBeepCallback(3);
    }
}

void LapTimer::stop() {
    DEBUG("LapTimer stopped\n");
    state = STOPPED;
    lapCountWraparound = false;
    lapCount = 0;
    rssiCount = 0;
    memset(lapTimes, 0, sizeof(lapTimes));
    
    // Звук зупинки - 800Hz 500мс
    buz->tone(800, 500);
    led->on(500);
    
    // Відправляємо подію зупинки на веб-сторінку
    if (raceFinishCallback) {
        raceFinishCallback();
    }
}

void LapTimer::handleLapTimerUpdate(uint32_t currentTimeMs) {
    // always read RSSI
    rssi[rssiCount] = round(filter.filter(rx->readRssi(), 0));
    // DEBUG("RSSI: %u\n", rssi[rssiCount]);

    switch (state) {
        case STOPPED:
            break;
        case COUNTDOWN:
            // Обробка countdown - біп кожні 1000мс (250мс звук + 750мс пауза)
            if ((currentTimeMs - countdownStartTime) >= 3000) {
                // Countdown закінчився - запускаємо гонку
                DEBUG("LapTimer race started!\n");
                
                // ВАЖЛИВО: Таймер починає відлік одразу коли почався звук старту
                raceStartTimeMs = currentTimeMs;
                state = RUNNING;
                
                // Звук старту 800Hz на 500мс
                buz->tone(800, 500);
                led->on(500);
                startLap();
                
                // Відправляємо подію старту на веб-сторінку
                if (raceStartCallback) {
                    raceStartCallback();
                }
            } else {
                // Час для наступного біпу? (через 1000мс після попереднього)
                uint32_t timeSinceStart = currentTimeMs - countdownStartTime;
                uint32_t nextBeepTime = (4 - countdownCounter) * 1000; // 1000мс, 2000мс, 3000мс
                
                if (timeSinceStart >= nextBeepTime && countdownCounter > 0) {
                    countdownCounter--;
                    
                    if (countdownCounter > 0) {
                        buz->tone(500, 250);  // 500Hz, 250мс - countdown біп
                        led->blink(250);
                        DEBUG("Countdown: %d\n", countdownCounter);
                        
                        // Відправляємо подію countdown на веб-сторінку
                        if (countdownBeepCallback) {
                            countdownBeepCallback(countdownCounter);
                        }
                    }
                }
            }
            break;
        case WAITING:
            // detect hole shot
            lapPeakCapture();
            if (lapPeakCaptured()) {
                state = RUNNING;
                startLap();
            }
            break;
        case RUNNING:
            // Check if timer min has elapsed, start capturing peak
            if ((currentTimeMs - startTimeMs) > conf->getMinLapMs()) {
                lapPeakCapture();
            }

            if (lapPeakCaptured()) {
                finishLap();
                startLap();
            }
            break;
        default:
            break;
    }

    rssiCount = (rssiCount + 1) % LAPTIMER_RSSI_HISTORY;
}

void LapTimer::lapPeakCapture() {
    // Check if RSSI is on or post threshold, update RSSI peak
    if (rssi[rssiCount] >= conf->getEnterRssi()) {
        // Check if RSSI is greater than the previous detected peak
        if (rssi[rssiCount] > rssiPeak) {
            rssiPeak = rssi[rssiCount];
            rssiPeakTimeMs = millis();
        }
    }
}

bool LapTimer::lapPeakCaptured() {
    return (rssi[rssiCount] < rssiPeak) && (rssi[rssiCount] < conf->getExitRssi());
}

void LapTimer::startLap() {
    DEBUG("Lap started\n");
    startTimeMs = rssiPeakTimeMs;
    rssiPeak = 0;
    rssiPeakTimeMs = 0;
    buz->beep(200);
    led->on(200);
}

void LapTimer::finishLap() {
    lapTimes[lapCount] = rssiPeakTimeMs - startTimeMs;
    if (lapCount == 0 && lapCountWraparound == false)
    {
        lapTimes[0] = rssiPeakTimeMs - raceStartTimeMs;
    }
    else
    {
        lapTimes[lapCount] = rssiPeakTimeMs - startTimeMs;
    }
    DEBUG("Lap finished, lap time = %u\n", lapTimes[lapCount]);
    
    // Звук фіксації кола - 500Hz 250мс
    buz->tone(500, 250);
    led->blink(250);
    
    // Відправляємо подію фіксації кола на веб-сторінку
    if (lapCompleteCallback) {
        lapCompleteCallback(lapCount, lapTimes[lapCount]);
    }
    
    if ((lapCount + 1) % LAPTIMER_LAP_HISTORY == 0) {
        lapCountWraparound = true;
    }
    lapCount = (lapCount + 1) % LAPTIMER_LAP_HISTORY;
    lapAvailable = true;
}

uint8_t LapTimer::getRssi() {
    return rssi[rssiCount];
}

uint32_t LapTimer::getLapTime() {
    uint32_t lapTime = 0;
    lapAvailable = false;
    if (lapCount == 0) {
        lapTime = lapTimes[LAPTIMER_LAP_HISTORY - 1];
    } else {
        lapTime = lapTimes[lapCount - 1];
    }
    return lapTime;
}

bool LapTimer::isLapAvailable() {
    return lapAvailable;
}

String LapTimer::getRaceStatus() {
    switch (state) {
        case STOPPED:
            return "Wait start";
        case COUNTDOWN:
            {
                unsigned long elapsed = millis() - countdownStartTime;
                int remaining = 3 - (elapsed / 1000);
                if (remaining > 0) {
                    return "Start " + String(remaining);
                } else {
                    return "GO!";
                }
            }
        case WAITING:
            return "Starting...";
        case RUNNING:
            if (lapCount == 0) {
                return "Lap0 started";
            } else {
                // Показуємо час останнього завершеного кола
                uint32_t lastLapTime = (lapCount == 0) ? lapTimes[LAPTIMER_LAP_HISTORY - 1] : lapTimes[lapCount - 1];
                float timeInSeconds = lastLapTime / 1000.0f;
                return "Lap" + String(lapCount) + ": " + String(timeInSeconds, 2) + "s";
            }
        default:
            return "";
    }
}

void LapTimer::setCountdownBeepCallback(void (*callback)(int countNumber)) {
    countdownBeepCallback = callback;
}

void LapTimer::setRaceStartCallback(void (*callback)()) {
    raceStartCallback = callback;
}

void LapTimer::setLapCompleteCallback(void (*callback)(int lapNumber, uint32_t lapTime)) {
    lapCompleteCallback = callback;
}

void LapTimer::setRaceFinishCallback(void (*callback)()) {
    raceFinishCallback = callback;
}
