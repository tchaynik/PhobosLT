#pragma once

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <stdint.h>

/*
## Pinout ##
| ESP32 | RX5880 |
| :------------- |:-------------|
| 33 | RSSI |
| GND | GND |
| 19 | CH1 |
| 22 | CH2 |
| 23 | CH3 |
| 3V3 | +5V |

* **Led** goes to pin 21 and GND
* The optional **Buzzer** goes to pin 25 or 27 and GND

*/

//ESP23-C3
#if defined(ESP32C3)

#define PIN_LED 4             // GPIO4 - STATUS LED
#define PIN_VBAT 10           // GPIO10 - Контроль батареї 
#define VBAT_SCALE 2
#define VBAT_ADD 2
#define PIN_RX5808_RSSI 9     // GPIO9 - Аналоговий сигнал RSSI
#define PIN_RX5808_DATA 6     // GPIO6 - Дані для RX5808
#define PIN_RX5808_SELECT 7   // GPIO7 - Вибір каналу RX5808  
#define PIN_RX5808_CLOCK 8    // GPIO8 - Тактування RX5808
#define PIN_BUZZER 5          // GPIO5 - П'єзо зумер
#define BUZZER_INVERTED false
#define PIN_OLED_SDA 2        // GPIO2 - I2C SDA для OLED
#define PIN_OLED_SCL 3        // GPIO3 - I2C SCL для OLED  
#define PIN_BUTTON_BOOT 0     // GPIO0 - Кнопка BOOT (таймер)
#define PIN_BUTTON_CHANNEL 1  // GPIO1 - Кнопка зміни каналу

//ESP32-S3
#elif defined(ESP32S3)

#define PIN_LED 2
#define PIN_VBAT 1
#define VBAT_SCALE 2
#define VBAT_ADD 2
#define PIN_RX5808_RSSI 13
#define PIN_RX5808_DATA 11     //CH1
#define PIN_RX5808_SELECT 10   //CH2
#define PIN_RX5808_CLOCK 12    //CH3
#define PIN_BUZZER 3
#define BUZZER_INVERTED false

//ESP32
#else

#define PIN_LED 21
#define PIN_VBAT 35
#define VBAT_SCALE 2
#define VBAT_ADD 2
#define PIN_RX5808_RSSI 33
#define PIN_RX5808_DATA 19   //CH1
#define PIN_RX5808_SELECT 22 //CH2
#define PIN_RX5808_CLOCK 23  //CH3
#define PIN_BUZZER 27
#define BUZZER_INVERTED false

#endif

#define EEPROM_RESERVED_SIZE 256
#define CONFIG_MAGIC_MASK (0b11U << 30)
#define CONFIG_MAGIC (0b01U << 30)
#define CONFIG_VERSION 0U

#define EEPROM_CHECK_TIME_MS 1000

// FPV канали та частоти
struct FPVChannel {
    const char* band;
    int channel;
    int frequency;
};

// Масив всіх доступних каналів з частотами в MHz
static const FPVChannel FPV_CHANNELS[] = {
    // Racе band (R)
    {"R", 1, 5658},
    {"R", 2, 5695},
    {"R", 3, 5732},
    {"R", 4, 5769},
    {"R", 5, 5806},
    {"R", 6, 5843},
    {"R", 7, 5880},
    {"R", 8, 5917},
    
    // Band A
    {"A", 1, 5865},
    {"A", 2, 5845},
    {"A", 3, 5825},
    {"A", 4, 5805},
    {"A", 5, 5785},
    {"A", 6, 5765},
    {"A", 7, 5745},
    {"A", 8, 5725},
    
    // Band B
    {"B", 1, 5733},
    {"B", 2, 5752},
    {"B", 3, 5771},
    {"B", 4, 5790},
    {"B", 5, 5809},
    {"B", 6, 5828},
    {"B", 7, 5847},
    {"B", 8, 5866},
    
    // Band E
    {"E", 1, 5705},
    {"E", 2, 5685},
    {"E", 3, 5665},
    {"E", 4, 5645},
    {"E", 5, 5885},
    {"E", 6, 5905},
    {"E", 7, 5925},
    {"E", 8, 5945},
    
    // Band F
    {"F", 1, 5740},
    {"F", 2, 5760},
    {"F", 3, 5780},
    {"F", 4, 5800},
    {"F", 5, 5820},
    {"F", 6, 5840},
    {"F", 7, 5860},
    {"F", 8, 5880}
};

#define FPV_CHANNELS_COUNT (sizeof(FPV_CHANNELS) / sizeof(FPVChannel))

// Функція для отримання частоти по каналу
inline int getChannelFrequency(const String& channelInfo) {
    for (int i = 0; i < FPV_CHANNELS_COUNT; i++) {
        String ch = String(FPV_CHANNELS[i].band) + String(FPV_CHANNELS[i].channel);
        if (ch == channelInfo) {
            return FPV_CHANNELS[i].frequency;
        }
    }
    return 5658; // За замовчуванням R1
}

// Device operation modes for Master-Slave architecture
enum DeviceMode : uint8_t {
    MODE_STANDALONE = 0,  // Single node operation (default)
    MODE_MASTER = 1,      // Master node - coordinates race
    MODE_SLAVE = 2        // Slave node - reports to master
};

typedef struct {
    uint32_t version;
    uint16_t frequency;
    uint8_t minLap;
    uint8_t alarm;
    uint8_t announcerType;
    uint8_t announcerRate;
    uint8_t enterRssi;
    uint8_t exitRssi;
    char nodeId[21];        // Renamed from pilotName - can be pilot name or node identifier
    char ssid[33];
    char password[33];
    uint8_t wifiMode;       // 0=AP, 1=STA
    uint8_t batteryWarningLevel; // Поріг попередження про батарею (%)
    uint8_t deviceMode;     // DeviceMode: Standalone/Master/Slave
    char masterIP[16];      // IP address of Master node (for Slave mode)
    uint8_t nodeChannel;    // Channel assignment for this node (1-8)
} laptimer_config_t;

class Config {
   public:
    void init();
    void load();
    void write();
    void toJson(AsyncResponseStream& destination);
    void toJsonString(char* buf);
    void fromJson(JsonObject source);
    void handleEeprom(uint32_t currentTimeMs);

    // getters and setters
    uint16_t getFrequency();
    void setFrequency(uint16_t frequency);
    uint32_t getMinLapMs();
    uint8_t getAlarmThreshold();
    uint8_t getBatteryWarningLevel();
    uint8_t getEnterRssi();
    uint8_t getExitRssi();
    char* getSsid();
    char* getPassword();
    void setSsid(const char* ssid);
    void setPassword(const char* password);
    uint8_t getWiFiMode();
    void setWiFiMode(uint8_t mode);
    
    // Master-Slave architecture methods
    char* getNodeId();
    void setNodeId(const char* nodeId);
    DeviceMode getDeviceMode();
    void setDeviceMode(DeviceMode mode);
    char* getMasterIP();
    void setMasterIP(const char* ip);
    uint8_t getNodeChannel();
    void setNodeChannel(uint8_t channel);

   private:
    laptimer_config_t conf;
    bool modified;
    volatile uint32_t checkTimeMs = 0;
    void setDefaults();
};
