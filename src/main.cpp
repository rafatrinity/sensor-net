// src/main.cpp
#include "config.hpp"
#include "network/wifi.hpp"
#include "network/mqttManager.hpp"
#include "sensors/sensorManager.hpp"
#include "actuators/actuatorManager.hpp"
#include "data/targetDataManager.hpp"
#include "utils/timeService.hpp"
#include "ui/displayManager.hpp"
#include "network/webServerManager.hpp"
#include "data/dataHistoryManager.hpp"
#include "utils/logger.hpp"

#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

volatile bool g_bleCredentialsReceived = false;
char g_receivedSsid[32];
char g_receivedPassword[64];

// --- Instâncias Principais (Managers Globais) ---
AppConfig appConfig;
GrowController::TargetDataManager targetManager;
GrowController::TimeService timeService;
GrowController::DataHistoryManager dataHistoryMgr;

GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS, timeService);
GrowController::MqttManager mqttMgr(appConfig.mqtt, targetManager);
GrowController::SensorManager sensorMgr(appConfig.sensor, timeService, &dataHistoryMgr, &displayMgr, &mqttMgr);
GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr, timeService);
GrowController::WebServerManager webServerManager(HTTP_PORT, &sensorMgr, &targetManager, &actuatorMgr, &dataHistoryMgr);

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool bleAdvertising = false;

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-123456789abc"

bool displayOk = false;
bool sensorsOk = false;
bool actuatorsOk = false;
bool wifiOk = false;
bool timeOk = false;
bool mqttSetupOk = false;
bool mqttTaskOk = false;
bool sensorTaskOk = false;
bool actuatorTasksOk = false;
bool littleFsOk = false;
bool dataHistoryOk = false;


class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) override {
        std::string value = pChar->getValue();
        if (value.length() > 0) {
            GrowController::Logger::info("BLE: Data received: %s", value.c_str());
            JsonDocument doc; // Usar JsonDocument diretamente (ArduinoJson v7+)
            DeserializationError error = deserializeJson(doc, value);
            if (error) {
                GrowController::Logger::error("BLE: JSON parsing failed: %s", error.c_str());
                pChar->setValue("Error: Invalid JSON");
                return;
            }
            const char* ssid_ble = doc["ssid"];
            const char* password_ble = doc["password"];
            if (ssid_ble && password_ble) {
                GrowController::Logger::info("BLE: SSID: %s, Password: %s", ssid_ble, password_ble);
                saveWiFiCredentials(ssid_ble, password_ble);
                GrowController::Logger::info("BLE: WiFi credentials saved. Restarting device.");
                g_bleCredentialsReceived = true;
                ESP.restart();
            } else {
                GrowController::Logger::error("BLE: SSID or password missing in JSON.");
                pChar->setValue("Error: Incomplete data");
            }
        } else {
            GrowController::Logger::warn("BLE: No data received on write.");
        }
    }
};

void activatePairingMode() {
    if (bleAdvertising) {
        GrowController::Logger::info("BLE pairing mode already active.");
        return;
    }
    GrowController::Logger::info("Activating BLE pairing mode.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    BLEDevice::init("GrowControllerCFG");
    pServer = BLEDevice::createServer();

    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
                      );
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());
    pCharacteristic->setValue("Send WiFi JSON: {\"ssid\":\"yourSSID\",\"password\":\"yourPASS\"}");

    pService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    bleAdvertising = true;
    GrowController::Logger::info("BLE: Advertising started. Waiting for credentials...");
    if (displayOk) {
        displayMgr.clear();
        displayMgr.printLine(0, "BLE Config Mode");
        displayMgr.printLine(1, "Send WiFi Creds");
    }
}


void setup() {
    Serial.begin(BAUD);
    GrowController::Logger::init(Serial, GrowController::LogLevel::INFO);
    GrowController::Logger::info("\n--- Booting Application ---");
    GrowController::Logger::info("Board: %s", BOARD_NAME);
    GrowController::Logger::info("Firmware Version: %s %s", __DATE__, __TIME__);

    pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP);

    Wire.begin(SDA, SCL);

    GrowController::Logger::info("Initializing Display Manager...");
    displayOk = displayMgr.initialize();
    if (!displayOk) GrowController::Logger::error("FATAL: Display Manager Initialization Failed!");
    else displayMgr.showBooting();

    GrowController::Logger::info("Initializing LittleFS...");
    if (!LittleFS.begin(true)) {
        GrowController::Logger::error("LittleFS Mount Failed!");
        if (displayOk) displayMgr.showError("FS Mount Fail");
        littleFsOk = false;
    } else {
        GrowController::Logger::info("LittleFS Mounted successfully.");
        littleFsOk = true;
    }

    if (littleFsOk) {
        GrowController::Logger::info("Initializing Data History Manager...");
        dataHistoryOk = dataHistoryMgr.initialize("hist_mgr_v1");
        if (!dataHistoryOk) {
            GrowController::Logger::error("Data History Manager Initialization Failed!");
            if (displayOk) displayMgr.showError("Hist Init Fail");
        } else {
            GrowController::Logger::info("Data History Manager Initialized. Records: %d", dataHistoryMgr.getRecordCount());
        }
    } else {
        GrowController::Logger::warn("Skipping Data History Manager initialization (LittleFS not OK).");
        dataHistoryOk = false;
    }

    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
        activatePairingMode();
    } else {
        GrowController::Logger::info("Starting WiFi Task...");
        WiFiTaskParams wifiParams = { &appConfig.wifi, (displayOk ? &displayMgr : nullptr) };
        BaseType_t wifiTaskResult = xTaskCreate( connectToWiFi, "WiFiTask", 4096, (void *)&wifiParams, 2, NULL );
        if (wifiTaskResult != pdPASS) {
            GrowController::Logger::error("FATAL ERROR: Failed to start WiFi Task! Code: %d", wifiTaskResult);
            if (displayOk) displayMgr.showError("Task WiFi Fail");
            while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
        }

        GrowController::Logger::info("Waiting for WiFi connection from task...");
        if (displayOk) displayMgr.showConnectingWiFi();
        uint32_t wifiWaitStart = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - wifiWaitStart < 30000)) {
            vTaskDelay(pdMS_TO_TICKS(250));
            if (displayOk) displayMgr.updateSpinner();
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifiOk = true;
            GrowController::Logger::info("WiFi Connected! IP Address: %s", WiFi.localIP().toString().c_str());
            if (displayOk) displayMgr.showWiFiConnected(WiFi.localIP().toString().c_str());
        } else {
            wifiOk = false;
            GrowController::Logger::error("WiFi Connection Failed (Timeout or error in task)!");
            if (displayOk) displayMgr.showError("WiFi Fail");
        }
    }

    if (wifiOk) {
        GrowController::Logger::info("Configuring mDNS responder...");
        if (MDNS.begin(MDNS_HOSTNAME)) {
            GrowController::Logger::info("MDNS responder started. Access at: http://%s.local", MDNS_HOSTNAME);
            MDNS.addService("http", "tcp", HTTP_PORT);
        } else {
            GrowController::Logger::error("Error setting up MDNS responder!");
            if (displayOk) displayMgr.showError("mDNS Fail");
        }
    } else if (!bleAdvertising) {
        GrowController::Logger::warn("Skipping mDNS setup (No WiFi).");
    }


    if (wifiOk && littleFsOk) {
        GrowController::Logger::info("Starting Web Server...");
        webServerManager.begin();
    } else if (!bleAdvertising) {
        GrowController::Logger::warn("Skipping Web Server start (No WiFi or LittleFS not mounted).");
    }

    if (!bleAdvertising) {
        GrowController::Logger::info("Setting up MQTT Manager...");
        mqttMgr.setup();
        mqttSetupOk = true;

        GrowController::Logger::info("Initializing Sensor Manager...");
        sensorsOk = sensorMgr.initialize();
        if (!sensorsOk) {
            GrowController::Logger::error("Sensor Manager Initialization Failed!");
            if (displayOk) displayMgr.showError("SensorInitFail");
        }

        GrowController::Logger::info("Initializing Actuator Manager...");
        actuatorsOk = actuatorMgr.initialize();
        if (!actuatorsOk) {
            GrowController::Logger::error("Actuator Manager Initialization Failed!");
            if (displayOk) displayMgr.showError("ActuatorIFail");
        }

        if (wifiOk) {
            if (displayOk) displayMgr.showNtpSyncing();
            timeOk = timeService.initialize(appConfig.time);
            if (!timeOk) {
                GrowController::Logger::error("Failed to configure Time Service (e.g. invalid NTP server string)!");
                if (displayOk) displayMgr.showError("NTP Cfg Fail");
            } else {
                struct tm timeinfo_local_var;
                if (timeService.getCurrentTime(timeinfo_local_var)) {
                    if (displayOk) displayMgr.showNtpSynced();
                    GrowController::Logger::info("Time Service initial sync successful.");
                } else {
                    if (displayOk) displayMgr.showError("NTP SyncPend");
                    GrowController::Logger::warn("Time Service configured, but initial sync pending/failed.");
                }
            }
        } else {
            GrowController::Logger::warn("Skipping Time Service initialization (No WiFi).");
            timeOk = false;
        }

        if (wifiOk && mqttSetupOk) {
            GrowController::Logger::info("Starting MQTT Task...");
            if (displayOk) displayMgr.showMqttConnecting();
            BaseType_t mqttTaskResult = xTaskCreate( GrowController::MqttManager::taskRunner, "MQTTTask", 4096, &mqttMgr, 2, nullptr );
            mqttTaskOk = (mqttTaskResult == pdPASS);
            if (!mqttTaskOk) {
                GrowController::Logger::error("Failed to start MQTT Task! Code: %d", mqttTaskResult);
                if (displayOk) displayMgr.showError("Task MQTT Fail");
            }
        } else {
            GrowController::Logger::warn("Skipping MQTT Task start (No WiFi or MQTT Setup failed).");
            mqttTaskOk = false;
        }

        if (sensorsOk) {
            GrowController::Logger::info("Starting Sensor Reading Task...");
            sensorTaskOk = sensorMgr.startSensorTask(1, 4096);
            if (!sensorTaskOk) {
                GrowController::Logger::error("Failed to start Sensor Task!");
                if (displayOk) displayMgr.showError("Task Sens Fail");
            }
        } else {
            GrowController::Logger::warn("Skipping Sensor Task start (Sensor Init failed).");
            sensorTaskOk = false;
        }

        bool canStartActuatorTasks = actuatorsOk && timeService.isInitialized() && sensorMgr.isInitialized();
        if (canStartActuatorTasks) {
            GrowController::Logger::info("Starting Actuator Control Tasks...");
            actuatorTasksOk = actuatorMgr.startControlTasks(1, 1, 2560);
            if (!actuatorTasksOk) {
                GrowController::Logger::error("Failed to start one or both Actuator Tasks!");
                if (displayOk) displayMgr.showError("Task Act Fail");
            }
        } else {
            GrowController::Logger::warn("Skipping Actuator Tasks start (Prerequisites not met: Act:%d, TimeCfg:%d, Sens:%d).",
                actuatorsOk, timeService.isInitialized(), sensorMgr.isInitialized());
            actuatorTasksOk = false;
        }
    }

    GrowController::Logger::info("--- Setup Complete ---");
    GrowController::Logger::info("Free Heap: %u bytes", ESP.getFreeHeap());

    if (displayOk && !bleAdvertising) {
        bool systemDegraded = !littleFsOk || !dataHistoryOk || !sensorsOk || !actuatorsOk || !wifiOk || !mqttTaskOk || !sensorTaskOk || !actuatorTasksOk;
        if (systemDegraded) {
             displayMgr.printLine(0, "System Started");
             displayMgr.printLine(1, wifiOk ? WiFi.localIP().toString().c_str() : "Degraded Mode");
        } else if (wifiOk && littleFsOk) {
             displayMgr.printLine(0, "System OK");
        }
    } else if (displayOk && bleAdvertising) {
        // Display handled in activatePairingMode
    }
}

void loop() {
    static unsigned long lastButtonCheck = 0;
    static bool buttonWasPressedState = false;
    unsigned long currentLoopMillis = millis();

    if (currentLoopMillis - lastButtonCheck > 100) {
        bool buttonIsCurrentlyPressed = (digitalRead(PAIRING_BUTTON_PIN) == LOW);
        if (buttonIsCurrentlyPressed && !buttonWasPressedState) {
            GrowController::Logger::info("Pairing button pressed. Activating BLE pairing mode.");
            activatePairingMode();
        }
        buttonWasPressedState = buttonIsCurrentlyPressed;
        lastButtonCheck = currentLoopMillis;
    }

    static unsigned long lastSseSendTime = 0;
    if (wifiOk && littleFsOk && !bleAdvertising && (currentLoopMillis - lastSseSendTime > 2000)) {
        webServerManager.sendSensorUpdateEvent();
        webServerManager.sendStatusUpdateEvent();
        lastSseSendTime = currentLoopMillis;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}