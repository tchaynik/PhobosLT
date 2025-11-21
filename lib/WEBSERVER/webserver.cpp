#include "webserver.h"
#include <ElegantOTA.h>

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <esp_wifi.h>

#include "debug.h"

static const uint8_t DNS_PORT = 53;
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress;
static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static const char *wifi_hostname = "plt";
static const char *wifi_ap_ssid_prefix = "PhobosLT";
static char wifi_ap_password[64] = "phoboslt";
static const char *wifi_ap_address = "20.0.0.1";
String wifi_ap_ssid;

void Webserver::init(Config *config, LapTimer *lapTimer, BatteryMonitor *batMonitor, Buzzer *buzzer, Led *l, OledDisplay *oledDisplay, ButtonHandler *buttonHandler) {

    ipAddress.fromString(wifi_ap_address);

    conf = config;
    timer = lapTimer;
    monitor = batMonitor;
    buz = buzzer;
    led = l;
    oled = oledDisplay;
    buttons = buttonHandler;

    wifi_ap_ssid = String(wifi_ap_ssid_prefix) + "_" + WiFi.macAddress().substring(WiFi.macAddress().length() - 6);
    wifi_ap_ssid.replace(":", "");

    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
    if (conf->getSsid()[0] == 0) {
        changeMode = WIFI_AP;
    } else {
        changeMode = WIFI_STA;
    }
    changeTimeMs = millis();
    lastStatus = WL_DISCONNECTED;
}

void Webserver::sendRssiEvent(uint8_t rssi) {
    if (!servicesStarted) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", rssi);
    events.send(buf, "rssi");
}

void Webserver::sendLaptimeEvent(uint32_t lapTime) {
    if (!servicesStarted) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", lapTime);
    events.send(buf, "lap");
}

void Webserver::sendCountdownBeepEvent(int countNumber) {
    if (!servicesStarted) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", countNumber);
    events.send(buf, "countdown");
}

void Webserver::sendRaceStartEvent() {
    if (!servicesStarted) return;
    events.send("start", "race");
}

void Webserver::sendLapCompleteEvent(int lapNumber, uint32_t lapTime) {
    if (!servicesStarted) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"lap\":%d,\"time\":%u}", lapNumber, lapTime);
    events.send(buf, "lapComplete");
}

void Webserver::sendRaceFinishEvent() {
    if (!servicesStarted) return;
    events.send("finish", "race");
}

void Webserver::sendBatteryWarningEvent(float voltage, int percentage) {
    if (!servicesStarted) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"voltage\":%.1f,\"percentage\":%d}", voltage, percentage);
    events.send(buf, "batteryWarning");
}

void Webserver::handleWebUpdate(uint32_t currentTimeMs) {
    if (timer->isLapAvailable()) {
        sendLaptimeEvent(timer->getLapTime());
    }

    if (sendRssi && ((currentTimeMs - rssiSentMs) > WEB_RSSI_SEND_TIMEOUT_MS)) {
        sendRssiEvent(timer->getRssi());
        rssiSentMs = currentTimeMs;
    }
    
    // Master mode: cleanup inactive nodes
    if (conf->getDeviceMode() == MODE_MASTER) {
        cleanupInactiveNodes(currentTimeMs);
    }

    // Перевіряємо батарею кожну хвилину
    if ((currentTimeMs - lastBatteryCheckMs) >= BATTERY_CHECK_INTERVAL_MS) {
        float voltage = monitor->getBatteryVoltage() / 10.0; // getBatteryVoltage повертає у десятках міліволь
        
        // Обчислюємо відсотки заряду (3.0V-4.2V діапазон Li-Ion)
        float minVoltage = 3.0;
        float maxVoltage = 4.2;
        int percentage = (int)((voltage - minVoltage) / (maxVoltage - minVoltage) * 100);
        
        // Обмежуємо діапазон
        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;
        
        // Відправляємо попередження якщо заряд нижче порогу і минуло достатньо часу з останнього попередження
        if (percentage <= conf->getBatteryWarningLevel() && 
            (currentTimeMs - lastBatteryWarningMs) >= BATTERY_WARNING_COOLDOWN_MS) {
            DEBUG("Battery low: %.1fV (%d%%) - sending warning\n", voltage, percentage);
            sendBatteryWarningEvent(voltage, percentage);
            buz->beep(100); // Короткий біп на пристрої
            lastBatteryWarningMs = currentTimeMs; // Запам'ятовуємо час останнього попередження
        }
        
        lastBatteryCheckMs = currentTimeMs;
    }

    wl_status_t status = WiFi.status();

    if (status != lastStatus && wifiMode == WIFI_STA) {
        DEBUG("WiFi status = %u\n", status);
        switch (status) {
            case WL_NO_SSID_AVAIL:
            case WL_CONNECT_FAILED:
            case WL_CONNECTION_LOST:
                changeTimeMs = currentTimeMs;
                changeMode = WIFI_AP;
                break;
            case WL_DISCONNECTED:  // try reconnection
                changeTimeMs = currentTimeMs;
                break;
            case WL_CONNECTED:
                buz->beep(200);
                led->off();
                wifiConnected = true;
                updateOledDisplay();  // Оновлюємо OLED при підключенні
                break;
            default:
                break;
        }
        lastStatus = status;
    }
    if (status != WL_CONNECTED && wifiMode == WIFI_STA && (currentTimeMs - changeTimeMs) > WIFI_CONNECTION_TIMEOUT_MS) {
        changeTimeMs = currentTimeMs;
        if (!wifiConnected) {
            changeMode = WIFI_AP;  // if we didnt manage to ever connect to wifi network
        } else {
            DEBUG("WiFi Connection failed, reconnecting\n");
            WiFi.reconnect();
            startServices();
            buz->beep(100);
            led->blink(200);
        }
    }
    if (changeMode != wifiMode && changeMode != WIFI_OFF && (currentTimeMs - changeTimeMs) > WIFI_RECONNECT_TIMEOUT_MS) {
        switch (changeMode) {
            case WIFI_AP:
                DEBUG("Changing to WiFi AP mode\n");

                WiFi.disconnect();
                wifiMode = WIFI_AP;
                WiFi.setHostname(wifi_hostname);  // hostname must be set before the mode is set to STA
                WiFi.mode(wifiMode);
                changeTimeMs = currentTimeMs;
                WiFi.softAPConfig(ipAddress, ipAddress, netMsk);
                WiFi.softAP(wifi_ap_ssid.c_str(), wifi_ap_password);
                startServices();
                updateOledDisplay();  // Оновлюємо OLED при запуску AP
                buz->beep(1000);
                led->on(1000);
                break;
            case WIFI_STA:
                DEBUG("Connecting to WiFi network\n");
                wifiMode = WIFI_STA;
                WiFi.setHostname(wifi_hostname);  // hostname must be set before the mode is set to STA
                WiFi.mode(wifiMode);
                changeTimeMs = currentTimeMs;
                WiFi.begin(conf->getSsid(), conf->getPassword());
                startServices();
                led->blink(200);
            default:
                break;
        }

        changeMode = WIFI_OFF;
    }

    if (servicesStarted) {
        dnsServer.processNextRequest();
    }
}

/** Is this an IP? */
static boolean isIp(String str) {
    for (size_t i = 0; i < str.length(); i++) {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9')) {
            return false;
        }
    }
    return true;
}

/** IP to String? */
static String toStringIp(IPAddress ip) {
    String res = "";
    for (int i = 0; i < 3; i++) {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

static bool captivePortal(AsyncWebServerRequest *request) {
    extern const char *wifi_hostname;

    if (!isIp(request->host()) && request->host() != (String(wifi_hostname) + ".local")) {
        DEBUG("Request redirected to captive portal\n");
        request->redirect(String("http://") + toStringIp(request->client()->localIP()));
        return true;
    }
    return false;
}

static void handleRoot(AsyncWebServerRequest *request) {
    if (captivePortal(request)) {  // If captive portal redirect instead of displaying the page.
        return;
    }
    request->send(LittleFS, "/index.html", "text/html");
}

static void handleNotFound(AsyncWebServerRequest *request) {
    if (captivePortal(request)) {  // If captive portal redirect instead of displaying the error page.
        return;
    }
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += request->url();
    message += F("\nMethod: ");
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += request->args();
    message += F("\n");

    for (uint8_t i = 0; i < request->args(); i++) {
        message += String(F(" ")) + request->argName(i) + F(": ") + request->arg(i) + F("\n");
    }
    AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", message);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
}

static bool startLittleFS() {
    if (!LittleFS.begin()) {
        DEBUG("LittleFS mount failed\n");
        return false;
    }
    DEBUG("LittleFS mounted sucessfully\n");
    return true;
}

static void startMDNS() {
    if (!MDNS.begin(wifi_hostname)) {
        DEBUG("Error starting mDNS\n");
        return;
    }

    String instance = String(wifi_hostname) + "_" + WiFi.macAddress();
    instance.replace(":", "");
    MDNS.setInstanceName(instance);
    MDNS.addService("http", "tcp", 80);
}

void Webserver::startServices() {
    if (servicesStarted) {
        MDNS.end();
        startMDNS();
        return;
    }

    startLittleFS();

    server.on("/", handleRoot);
    server.on("/generate_204", handleRoot);  // handle Andriod phones doing shit to detect if there is 'real' internet and possibly dropping conn.
    server.on("/gen_204", handleRoot);
    server.on("/library/test/success.html", handleRoot);
    server.on("/hotspot-detect.html", handleRoot);
    server.on("/connectivity-check.html", handleRoot);
    server.on("/check_network_status.txt", handleRoot);
    server.on("/ncsi.txt", handleRoot);
    server.on("/fwlink", handleRoot);

    server.on("/status", [this](AsyncWebServerRequest *request) {
        char buf[1024];
        char configBuf[256];
        conf->toJsonString(configBuf);
        float voltage = (float)monitor->getBatteryVoltage() / 10;
        const char *format =
            "\
Heap:\n\
\tFree:\t%i\n\
\tMin:\t%i\n\
\tSize:\t%i\n\
\tAlloc:\t%i\n\
LittleFS:\n\
\tUsed:\t%i\n\
\tTotal:\t%i\n\
Chip:\n\
\tModel:\t%s Rev %i, %i Cores, SDK %s\n\
\tFlashSize:\t%i\n\
\tFlashSpeed:\t%iMHz\n\
\tCPU Speed:\t%iMHz\n\
Network:\n\
\tIP:\t%s\n\
\tMAC:\t%s\n\
EEPROM:\n\
%s\n\
Battery Voltage:\t%0.1fv";

        snprintf(buf, sizeof(buf), format,
                 ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap(), LittleFS.usedBytes(), LittleFS.totalBytes(),
                 ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getSdkVersion(), ESP.getFlashChipSize(), ESP.getFlashChipSpeed() / 1000000, getCpuFrequencyMhz(),
                 WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), configBuf, voltage);
        request->send(200, "text/plain", buf);
        led->on(200);
    });

    server.on("/timer/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        timer->start();
        request->send(200, "application/json", "{\"status\": \"OK\"}");
    });

    server.on("/timer/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        timer->stop();
        request->send(200, "application/json", "{\"status\": \"OK\"}");
    });

    server.on("/timer/rssiStart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        sendRssi = true;
        request->send(200, "application/json", "{\"status\": \"OK\"}");
        led->on(200);
    });

    server.on("/timer/rssiStop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        sendRssi = false;
        request->send(200, "application/json", "{\"status\": \"OK\"}");
        led->on(200);
    });

    server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        conf->toJson(*response);
        request->send(response);
        led->on(200);
    });

    AsyncCallbackJsonWebHandler *configJsonHandler = new AsyncCallbackJsonWebHandler("/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
#ifdef DEBUG_OUT
        serializeJsonPretty(jsonObj, DEBUG_OUT);
        DEBUG("\n");
#endif
        conf->fromJson(jsonObj);
        request->send(200, "application/json", "{\"status\": \"OK\"}");
        led->on(200);
    });

    server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=600");

    events.onConnect([this](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            DEBUG("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        client->send("start", NULL, millis(), 1000);
        led->on(200);
    });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "600");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

    // WiFi API endpoints
    server.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        
        if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
            doc["mode"] = "AP";
            doc["ssid"] = wifi_ap_ssid;
            doc["ip"] = WiFi.softAPIP().toString();
            doc["signal"] = nullptr;
        } else {
            doc["mode"] = "STA";
            doc["ssid"] = WiFi.SSID();
            doc["ip"] = WiFi.localIP().toString();
            doc["signal"] = String(WiFi.RSSI()) + "dBm";
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray networks = doc["networks"].to<JsonArray>();
        
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; ++i) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = WiFi.SSID(i);
            network["rssi"] = WiFi.RSSI(i);
            network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/wifi/config", HTTP_POST, [this](AsyncWebServerRequest *request) {
        // This will be handled by the body handler below
    }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (char*)data, len);
        
        if (error) {
            request->send(400, "application/json", "{\"success\": false, \"error\": \"Invalid JSON\"}");
            return;
        }

        String mode = doc["mode"];
        String ssid = doc["ssid"];
        String password = doc["password"];
        String apPassword = doc["apPassword"];

        if (mode == "STA") {
            conf->setSsid(ssid.c_str());
            conf->setPassword(password.c_str());
            conf->setWiFiMode(WIFI_STA);
        } else {
            conf->setWiFiMode(WIFI_AP);
            if (apPassword.length() > 0) {
                // Set AP password if provided
                strncpy(wifi_ap_password, apPassword.c_str(), sizeof(wifi_ap_password) - 1);
            }
        }

        request->send(200, "application/json", "{\"success\": true}");
        
        // Schedule restart after response is sent
        delay(100);
        ESP.restart();
    });

    server.on("/api/wifi/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        conf->setSsid("");
        conf->setPassword("");
        conf->setWiFiMode(WIFI_AP);
        strcpy(wifi_ap_password, "");
        
        request->send(200, "application/json", "{\"success\": true}");
        
        delay(100);
        ESP.restart();
    });

    server.on("/api/system/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"success\": true}");
        delay(100);
        ESP.restart();
    });

    // Battery API endpoint
    server.on("/api/battery/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        
        float voltage = monitor->getBatteryVoltage() / 10.0;
        
        // Обчислюємо відсотки заряду (3.0V-4.2V діапазон Li-Ion)
        float minVoltage = 3.0;
        float maxVoltage = 4.2;
        int percentage = (int)((voltage - minVoltage) / (maxVoltage - minVoltage) * 100);
        
        // Обмежуємо діапазон
        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;
        
        // Ступінчасті рівні: 0, 25, 50, 75, 100
        int stepPercentage = 0;
        if (percentage >= 87) stepPercentage = 100;
        else if (percentage >= 62) stepPercentage = 75;
        else if (percentage >= 37) stepPercentage = 50;
        else if (percentage >= 12) stepPercentage = 25;
        else stepPercentage = 0;
        
        doc["voltage"] = round(voltage * 10) / 10.0; // Округлюємо до 0.1V
        doc["percentage"] = percentage;
        doc["stepPercentage"] = stepPercentage;
        doc["status"] = (voltage < 3.3) ? "low" : (voltage > 4.1) ? "full" : "normal";
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.onNotFound(handleNotFound);
    
    // Master mode API endpoints
    if (conf->getDeviceMode() == MODE_MASTER) {
        setupMasterAPI();
    }

    server.addHandler(&events);
    server.addHandler(configJsonHandler);

    ElegantOTA.setAutoReboot(true);
    ElegantOTA.begin(&server);

    server.begin();

    dnsServer.start(DNS_PORT, "*", ipAddress);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

    startMDNS();

    servicesStarted = true;
}

void Webserver::updateOledDisplay() {
    if (!oled || !oled->isInitialized()) return;
    
    String ssid = "";
    String ip = "";
    String channel_info = "R1";  // За замовчуванням
    String raceStatus = "";
    bool blinkBand = false;
    wifi_mode_t mode = WiFi.getMode();
    
    // Отримуємо інформацію про канал від ButtonHandler
    if (buttons) {
        channel_info = buttons->getChannelInfo();
        blinkBand = buttons->isBandModeActive();
    }
    
    // Отримуємо статус гонки від LapTimer
    bool timerActive = false;
    if (timer) {
        raceStatus = timer->getRaceStatus();
        // Таймер активний якщо стан не "Wait start" або "Stopped"
        timerActive = (raceStatus != "Wait start" && raceStatus != "Stopped");
        
        // Оновлюємо стан таймера в кнопках
        if (buttons) {
            buttons->setTimerActive(timerActive);
        }
    }
    
    if (mode == WIFI_AP) {
        ssid = wifi_ap_ssid;
        ip = WiFi.softAPIP().toString();
    } else if (mode == WIFI_STA && WiFi.status() == WL_CONNECTED) {
        ssid = WiFi.SSID();
        ip = WiFi.localIP().toString();
    } else {
        // WiFi не підключений
        oled->displayMessage("PhobosLT", "WiFi: OFF", 
                           channel_info.length() > 0 ? ("CH:" + channel_info) : "", "");
        return;
    }
    
    oled->displayWiFiInfo(ssid, ip, mode, channel_info, blinkBand, raceStatus, timerActive, monitor->getBatteryVoltage() / 10.0);
}

// Master mode API implementation
void Webserver::setupMasterAPI() {
    // Node registration endpoint
    server.on("/api/node/register", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleNodeRegistration(request);
    });
    
    // Node heartbeat endpoint  
    server.on("/api/node/heartbeat", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleNodeHeartbeat(request);
    });
    
    // Node detection report endpoint
    server.on("/api/node/detection", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleNodeDetection(request);
    });
    
    // Get registered nodes list
    server.on("/api/nodes/list", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray nodes = doc["nodes"].to<JsonArray>();
        
        for (auto& pair : registeredNodes) {
            SlaveNode& node = pair.second;
            JsonObject nodeObj = nodes.add<JsonObject>();
            nodeObj["nodeId"] = node.nodeId;
            nodeObj["ipAddress"] = node.ipAddress;
            nodeObj["channel"] = node.channel;
            nodeObj["isActive"] = node.isActive;
            nodeObj["totalLaps"] = node.totalLaps;
            nodeObj["lastLapTime"] = node.lastLapTime;
            nodeObj["lastHeartbeat"] = node.lastHeartbeat;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Remove node endpoint
    server.on("/api/nodes/remove", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("nodeId", true)) {
            String nodeId = request->getParam("nodeId", true)->value();
            registeredNodes.erase(nodeId);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"nodeId required\"}");
        }
    });
    
    // Broadcast race start to all nodes
    server.on("/api/race/broadcast_start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        broadcastRaceCommand("start");
        request->send(200, "application/json", "{\"success\":true}");
    });
    
    // Broadcast race stop to all nodes
    server.on("/api/race/broadcast_stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        broadcastRaceCommand("stop");
        request->send(200, "application/json", "{\"success\":true}");
    });
}

void Webserver::handleNodeRegistration(AsyncWebServerRequest *request) {
    // Validate required parameters
    if (!request->hasParam("nodeId", true) || !request->hasParam("channel", true)) {
        JsonDocument errorDoc;
        errorDoc["error"] = "nodeId and channel required";
        errorDoc["message"] = "Please provide both Node ID and Channel parameters";
        
        String errorResponse;
        serializeJson(errorDoc, errorResponse);
        request->send(400, "application/json", errorResponse);
        return;
    }
    
    String nodeId = request->getParam("nodeId", true)->value();
    String channelStr = request->getParam("channel", true)->value();
    String clientIP = request->client()->remoteIP().toString();
    
    // Validate nodeId is not empty
    if (nodeId.length() == 0) {
        JsonDocument errorDoc;
        errorDoc["error"] = "nodeId cannot be empty";
        errorDoc["message"] = "Please provide a valid Node ID";
        
        String errorResponse;
        serializeJson(errorDoc, errorResponse);
        request->send(400, "application/json", errorResponse);
        return;
    }
    
    // Validate channel is a valid number (1-8)
    uint8_t channel = channelStr.toInt();
    if (channel < 1 || channel > 8) {
        JsonDocument errorDoc;
        errorDoc["error"] = "Invalid channel";
        errorDoc["message"] = "Channel must be between 1 and 8";
        
        String errorResponse;
        serializeJson(errorDoc, errorResponse);
        request->send(400, "application/json", errorResponse);
        return;
    }
        
        // Check if maximum nodes limit reached (1 Master + 7 Slaves = 8 total)
        if (registeredNodes.size() >= 7) {
            DEBUG("Registration denied: Network full (%d/7 nodes). Request from %s (%s)\n", 
                  registeredNodes.size(), nodeId.c_str(), clientIP.c_str());
                  
            JsonDocument errorDoc;
            errorDoc["error"] = "Maximum 7 slave nodes allowed";
            errorDoc["message"] = "Master network is full. Contact race coordinator.";
            errorDoc["maxNodes"] = 7;
            errorDoc["currentNodes"] = (int)registeredNodes.size();
            
            String errorResponse;
            serializeJson(errorDoc, errorResponse);
            request->send(400, "application/json", errorResponse);
            return;
        }
        
        // Check if nodeId already exists
        if (registeredNodes.find(nodeId) != registeredNodes.end()) {
            // Update existing node
            registeredNodes[nodeId].ipAddress = clientIP;
            registeredNodes[nodeId].channel = channel;
            registeredNodes[nodeId].lastHeartbeat = millis();
            registeredNodes[nodeId].isActive = true;
            
            DEBUG("Node updated: %s @ %s (CH:%d)\n", nodeId.c_str(), clientIP.c_str(), channel);
        } else {
            // Register new node
            SlaveNode node;
            node.nodeId = nodeId;
            node.ipAddress = clientIP;
            node.channel = channel;
            node.lastHeartbeat = millis();
            node.isActive = true;
            node.totalLaps = 0;
            node.lastLapTime = 0;
            
            registeredNodes[nodeId] = node;
            
            DEBUG("Node registered: %s @ %s (CH:%d)\n", nodeId.c_str(), clientIP.c_str(), channel);
        }
        
        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Node registered successfully";
        doc["assignedChannel"] = channel;
        doc["nodeId"] = nodeId;
        doc["masterIP"] = WiFi.localIP().toString();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
}

void Webserver::handleNodeHeartbeat(AsyncWebServerRequest *request) {
    if (request->hasParam("nodeId", true)) {
        String nodeId = request->getParam("nodeId", true)->value();
        
        if (registeredNodes.find(nodeId) != registeredNodes.end()) {
            registeredNodes[nodeId].lastHeartbeat = millis();
            registeredNodes[nodeId].isActive = true;
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(404, "application/json", "{\"error\":\"node not registered\"}");
        }
    } else {
        request->send(400, "application/json", "{\"error\":\"nodeId required\"}");
    }
}

void Webserver::handleNodeDetection(AsyncWebServerRequest *request) {
    if (request->hasParam("nodeId", true) && 
        request->hasParam("lapTime", true)) {
        
        String nodeId = request->getParam("nodeId", true)->value();
        uint32_t lapTime = request->getParam("lapTime", true)->value().toInt();
        
        if (registeredNodes.find(nodeId) != registeredNodes.end()) {
            registeredNodes[nodeId].totalLaps++;
            registeredNodes[nodeId].lastLapTime = lapTime;
            registeredNodes[nodeId].lastHeartbeat = millis();
            
            // Send lap complete event to web interface
            sendLapCompleteEvent(registeredNodes[nodeId].totalLaps, lapTime);
            
            DEBUG("Lap detected: %s - Lap %d, Time: %dms\n", 
                  nodeId.c_str(), registeredNodes[nodeId].totalLaps, lapTime);
            
            request->send(200, "application/json", "{\"status\":\"recorded\"}");
        } else {
            request->send(404, "application/json", "{\"error\":\"node not registered\"}");
        }
    } else {
        request->send(400, "application/json", "{\"error\":\"nodeId and lapTime required\"}");
    }
}

void Webserver::cleanupInactiveNodes(uint32_t currentTimeMs) {
    const uint32_t HEARTBEAT_TIMEOUT = 60000; // 1 minute timeout
    
    for (auto& pair : registeredNodes) {
        SlaveNode& node = pair.second;
        if (currentTimeMs - node.lastHeartbeat > HEARTBEAT_TIMEOUT) {
            node.isActive = false;
        }
    }
}

void Webserver::broadcastRaceCommand(const String& command) {
    // Note: In a real implementation, we would send HTTP requests to all registered nodes
    // For now, we just log the command
    DEBUG("Broadcasting race command: %s to %d nodes\n", command.c_str(), registeredNodes.size());
    
    for (auto& pair : registeredNodes) {
        SlaveNode& node = pair.second;
        if (node.isActive) {
            DEBUG("  -> Sending to %s @ %s\n", node.nodeId.c_str(), node.ipAddress.c_str());
            // TODO: Implement HTTP client to send commands to slaves
        }
    }
}
