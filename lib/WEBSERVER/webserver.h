#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <map>

#include "buzzer.h"
#include "led.h"
#include "config.h"
#include "battery.h"
#include "laptimer.h"
#include "oled.h"
#include "buttons.h"

#define WIFI_CONNECTION_TIMEOUT_MS 30000
#define WIFI_RECONNECT_TIMEOUT_MS 500
#define WEB_RSSI_SEND_TIMEOUT_MS 200

// Structure for registered slave nodes (Master mode)
struct SlaveNode {
    String nodeId;          // Node identifier (pilot name)
    String ipAddress;       // IP address of slave node
    uint32_t lastHeartbeat; // Last heartbeat timestamp
    uint8_t channel;        // Assigned channel
    bool isActive;          // Is currently connected
    uint32_t totalLaps;     // Total laps completed
    uint32_t lastLapTime;   // Last lap time in milliseconds
};

class Webserver {
   public:
    void init(Config *config, LapTimer *lapTimer, BatteryMonitor *batMonitor, Buzzer *buzzer, Led *l, OledDisplay *oledDisplay = nullptr, ButtonHandler *buttonHandler = nullptr);
    void handleWebUpdate(uint32_t currentTimeMs);
    void updateOledDisplay(); // Публічний метод для оновлення OLED
    
    // Методи для відправки звукових подій на веб-сторінку
    void sendCountdownBeepEvent(int countNumber);  // countdown біп з номером (3, 2, 1)
    void sendRaceStartEvent();  // звук старту гонки
    void sendLapCompleteEvent(int lapNumber, uint32_t lapTime); // фіксація кола з часом
    void sendRaceFinishEvent(); // зупинка гонки
    void sendBatteryWarningEvent(float voltage, int percentage); // попередження про низький заряд

   private:
    void startServices();
    
    // Master-Slave support
    std::map<String, SlaveNode> registeredNodes;  // Registered slave nodes
    void setupMasterAPI();                        // Setup Master mode API endpoints
    void handleNodeRegistration(AsyncWebServerRequest *request);
    void handleNodeHeartbeat(AsyncWebServerRequest *request);
    void handleNodeDetection(AsyncWebServerRequest *request);
    void cleanupInactiveNodes(uint32_t currentTimeMs);
    void broadcastRaceCommand(const String& command);
    void sendRssiEvent(uint8_t rssi);
    void sendLaptimeEvent(uint32_t lapTime);

    Config *conf;
    LapTimer *timer;
    BatteryMonitor *monitor;
    Buzzer *buz;
    Led *led;
    OledDisplay *oled;
    ButtonHandler *buttons;

    wifi_mode_t wifiMode = WIFI_OFF;
    wl_status_t lastStatus = WL_IDLE_STATUS;
    volatile wifi_mode_t changeMode = WIFI_OFF;
    volatile uint32_t changeTimeMs = 0;
    bool servicesStarted = false;
    bool wifiConnected = false;

    bool sendRssi = false;
    uint32_t rssiSentMs = 0;
    
    // Змінні для перевірки батареї
    uint32_t lastBatteryCheckMs = 0;
    uint32_t lastBatteryWarningMs = 0;
    const uint32_t BATTERY_CHECK_INTERVAL_MS = 60000; // 1 хвилина
    const uint32_t BATTERY_WARNING_COOLDOWN_MS = 300000; // 5 хвилин між попередженнями
};
