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

#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

// Variáveis globais para sinalizar o recebimento de credenciais
volatile bool g_bleCredentialsReceived = false;
char g_receivedSsid[32];
char g_receivedPassword[64];

// --- Instâncias Principais (Managers Globais) ---
AppConfig appConfig; // appConfig deve ser preenchido com os valores de config.hpp
GrowController::TargetDataManager targetManager;
GrowController::TimeService timeService;

// Managers que dependem de outros devem ser declarados após suas dependências
GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS, timeService);
GrowController::MqttManager mqttMgr(appConfig.mqtt, targetManager);
GrowController::SensorManager sensorMgr(appConfig.sensor, &displayMgr, &mqttMgr);
GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr, timeService);
GrowController::WebServerManager webServerManager(HTTP_PORT, &sensorMgr, &targetManager, &actuatorMgr);

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-123456789abc"

// --- Flags de Status da Inicialização ---
bool displayOk = false;
bool sensorsOk = false;
bool actuatorsOk = false;
bool wifiOk = false;
bool timeOk = false;
bool mqttSetupOk = false;
bool mqttTaskOk = false;
bool sensorTaskOk = false;
bool actuatorTasksOk = false;
bool littleFsOk = false; // Nova flag para LittleFS


class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) override {
        Serial.println("Callback onWrite acionado.");
        std::string value = pChar->getValue();
        if (value.length() > 0) {
            Serial.println("BLE: Dados recebidos na característica:");
            Serial.println(value.c_str());
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, value);
            if (error) {
                Serial.print("BLE Error: Falha ao analisar JSON: ");
                Serial.println(error.c_str());
                pChar->setValue("Erro: JSON invalido");
                return;
            }
            const char* ssid_ble = doc["ssid"];
            const char* password_ble = doc["password"];
            if (ssid_ble && password_ble) {
                Serial.print("SSID Recebido via BLE: "); Serial.println(ssid_ble);
                Serial.print("Senha Recebida via BLE: "); Serial.println(password_ble);
                saveWiFiCredentials(ssid_ble, password_ble);
                Serial.println("Credenciais Wi-Fi salvas. Reiniciando o dispositivo para aplicar.");
                g_bleCredentialsReceived = true; // Embora não usado diretamente após, é bom manter
                // strncpy não é necessário aqui pois ESP.restart() virá em seguida.
                ESP.restart();
            } else {
                Serial.println("BLE Error: SSID ou senha ausentes nos dados JSON recebidos.");
                pChar->setValue("Erro: Dados incompletos");
            }
        } else {
            Serial.println("BLE: Nenhum dado recebido na característica.");
        }
    }
};

void activatePairingMode() {
    Serial.println("Modo de pareamento BLE ativado.");
    BLEDevice::init("SensorNet"); // Use um nome descritivo para o seu dispositivo
    pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_WRITE
                      );
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());
    // Configurações de segurança (opcional, mas bom para pareamento)
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND); // Exige pareamento
    pSecurity->setCapability(ESP_IO_CAP_NONE); // Sem capacidade de display/teclado no ESP32 para entrada de PIN
    pSecurity->setKeySize(16);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    pService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    Serial.println("Aguardando credenciais via BLE... (Callback configurado)");
    if (displayOk) {
        displayMgr.clear();
        displayMgr.printLine(0, "Modo Config BLE");
        displayMgr.printLine(1, "Envie WiFi Creds");
    }
}


void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application ---");

    pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP);

    // 1. Inicializa I2C
    Wire.begin(SDA, SCL);

    // 2. Inicializa Display Manager
    Serial.println("Initializing Display Manager...");
    displayOk = displayMgr.initialize();
    if (!displayOk) Serial.println("FATAL: Display Manager Initialization Failed!");
    else displayMgr.showBooting();

    // 3. Inicializa LittleFS
    Serial.println("Initializing LittleFS...");
    if (!LittleFS.begin(true)) { // true para formatar se não conseguir montar
        Serial.println("LittleFS Mount Failed!");
        if (displayOk) displayMgr.showError("FS Mount Fail");
        littleFsOk = false;
    } else {
        Serial.println("LittleFS Mounted successfully.");
        littleFsOk = true;
    }

    // Lista arquivos do LittleFS se montado com sucesso
    if (littleFsOk) {
        Serial.println("Listing files from LittleFS:");
        File root = LittleFS.open("/");
        if (!root) {
            Serial.println("- failed to open root directory");
        } else if (!root.isDirectory()) {
            Serial.println(" - root is not a directory");
        } else {
            File file = root.openNextFile();
            while(file){
                Serial.printf("  FILE: /%s (Size: %lu)\n", file.name(), file.size());
                file.close();
                file = root.openNextFile();
            }
        }
        if(root) root.close(); // Garante que root seja fechado se foi aberto
    }


    // Verifica se o botão está pressionado na inicialização ANTES de iniciar a tarefa WiFi
    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
        activatePairingMode();
        // A execução pode continuar aqui se o BLE não reiniciar o dispositivo
    }

    // 4. Inicia Tarefa WiFi
    Serial.println("Starting WiFi Task...");
    WiFiTaskParams wifiParams = { &appConfig.wifi, (displayOk ? &displayMgr : nullptr) };
    BaseType_t wifiTaskResult = xTaskCreate( connectToWiFi, "WiFiTask", 4096, (void *)&wifiParams, 1, NULL );
    if (wifiTaskResult != pdPASS) {
          Serial.println("FATAL ERROR: Failed to start WiFi Task!");
          if (displayOk) displayMgr.showError("Task WiFi Fail");
          // Considere uma forma de lidar com essa falha crítica, talvez um loop infinito com delay
          while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }


    // 5. Aguarda Conexão WiFi
    Serial.print("Waiting for WiFi connection from task...");
    if (displayOk) displayMgr.showConnectingWiFi();
    uint32_t wifiWaitStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiWaitStart < 60000)) { // Timeout de 60s
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        // O display é atualizado pela tarefa WiFi no sucesso
    } else {
        wifiOk = false;
        Serial.println("\nERROR: WiFi Connection Failed (Timeout or from task)!");
        if (displayOk) displayMgr.showError("WiFi Fail");
        // A tarefa connectToWiFi pode ter chamado activatePairingMode
    }

    // Configura mDNS se WiFi estiver conectado
    if (wifiOk) {
        Serial.println("Configuring mDNS responder...");
        if (MDNS.begin(MDNS_HOSTNAME)) {
            Serial.printf("MDNS responder started. You can now access the device at: http://%s.local\n", MDNS_HOSTNAME);
            MDNS.addService("http", "tcp", HTTP_PORT);
            Serial.printf("mDNS service _http._tcp announced on port %d\n", HTTP_PORT);
        } else {
            Serial.println("Error setting up MDNS responder!");
            if (displayOk) displayMgr.showError("mDNS Fail");
        }
    } else {
        Serial.println("Skipping mDNS setup (No WiFi).");
    }

    // 6. Inicializa WebServer (APÓS WiFi e LittleFS)
    if (wifiOk && littleFsOk) {
        Serial.println("Starting Web Server...");
        webServerManager.begin(); // WebServerManager::begin() deve verificar internamente se LittleFS está montado
        if (displayOk) {
             displayMgr.printLine(1, WiFi.localIP().toString().c_str());
        }
    } else {
        Serial.println("Skipping Web Server start (No WiFi or LittleFS not mounted).");
        if (displayOk && !littleFsOk) displayMgr.showError("Web Srv Skip");
        else if (displayOk && !wifiOk) displayMgr.showError("Web Srv Skip"); // Se WiFi falhou mas FS OK
    }

    // 7. Configura MqttManager
    Serial.println("Setting up MQTT Manager...");
    mqttMgr.setup();
    mqttSetupOk = true; // Assumindo que setup não falha criticamente (ele loga erros)

    // 8. Inicializar Sensor Manager
    Serial.println("Initializing Sensor Manager...");
    sensorsOk = sensorMgr.initialize();
    if (!sensorsOk) {
        Serial.println("ERROR: Sensor Manager Initialization Failed!");
        if (displayOk) displayMgr.showError("Sensor Init Fail");
    }

    // 9. Inicializar Actuator Manager
    Serial.println("Initializing Actuator Manager...");
    actuatorsOk = actuatorMgr.initialize();
    if (!actuatorsOk) {
        Serial.println("ERROR: Actuator Manager Initialization Failed!");
        if (displayOk) displayMgr.showError("Actuator InitFail");
    }

    // 10. Inicializa Time Service (APÓS WiFi)
    if (wifiOk) {
        if (displayOk) displayMgr.showNtpSyncing();
        timeOk = timeService.initialize(appConfig.time);
        if (!timeOk) { // initialize pode retornar false se config for inválida
            Serial.println("ERROR: Failed to configure Time Service (invalid config)!");
            if (displayOk) displayMgr.showError("NTP Cfg Fail");
        } else {
            struct tm timeinfo;
            if (timeService.getCurrentTime(timeinfo)) { // Verifica sincronização inicial
                if (displayOk) displayMgr.showNtpSynced();
                Serial.println("Time Service initial sync successful.");
            } else {
                if (displayOk) displayMgr.showError("NTP Sync Fail");
                Serial.println("WARN: Time Service configured, but initial sync failed.");
                // timeOk ainda pode ser true aqui, pois a configuração ocorreu, mas a sincronização não.
            }
        }
    } else {
        Serial.println("Skipping Time Service initialization (No WiFi).");
        timeOk = false;
    }

    // 11. Inicia Tarefa MQTT
    if (wifiOk && mqttSetupOk) {
        Serial.println("Starting MQTT Task...");
        if (displayOk) displayMgr.showMqttConnecting(); // DisplayManager pode precisar de um showMqttStatus(bool connected)
        BaseType_t mqttTaskResult = xTaskCreate( GrowController::MqttManager::taskRunner, "MQTTTask", 4096, &mqttMgr, 2, nullptr );
        mqttTaskOk = (mqttTaskResult == pdPASS);
        if (!mqttTaskOk) {
            Serial.println("ERROR: Failed to start MQTT Task!");
            if (displayOk) displayMgr.showError("Task MQTT Fail");
        }
    } else {
        Serial.println("Skipping MQTT Task start (No WiFi or MQTT Setup failed).");
        mqttTaskOk = false;
    }

    // 12. Inicia Tarefa de Leitura de Sensores
    if (sensorsOk) {
        Serial.println("Starting Sensor Reading Task...");
        sensorTaskOk = sensorMgr.startSensorTask(1, 4096);
        if (!sensorTaskOk) {
            Serial.println("ERROR: Failed to start Sensor Task!");
            if (displayOk) displayMgr.showError("Task Sens Fail");
        }
   } else {
        Serial.println("Skipping Sensor Task start (Sensor Init failed).");
        sensorTaskOk = false;
   }

    // 13. Inicia Tarefas de Controle de Atuadores
    // Verifica se os pré-requisitos para cada tarefa de atuador são atendidos
    bool canStartActuatorTasks = actuatorsOk && ( (timeOk && sensorMgr.isInitialized()) || sensorMgr.isInitialized() );
    if (canStartActuatorTasks) {
        Serial.println("Starting Actuator Control Tasks...");
        // A lógica dentro de startControlTasks deve lidar com o fato de que TimeService pode não estar sincronizado
        // ou SensorManager pode não ter leituras válidas inicialmente.
        actuatorTasksOk = actuatorMgr.startControlTasks(1, 1, 2560);
        if (!actuatorTasksOk) {
             Serial.println("ERROR: Failed to start one or both Actuator Tasks!");
             if (displayOk) displayMgr.showError("Task Act Fail");
        } else {
             Serial.println("Actuator Tasks started.");
        }
    } else {
        Serial.println("Skipping Actuator Tasks start (Actuators not OK, or Time/Sensors not ready for all tasks).");
        actuatorTasksOk = false;
    }

    Serial.println("--- Setup Complete ---");
    Serial.println("Initialization Status:");
    Serial.printf("  Display:     %s\n", displayOk ? "OK" : "FAILED");
    Serial.printf("  LittleFS:    %s\n", littleFsOk ? "OK" : "FAILED");
    Serial.printf("  WiFi:        %s (IP: %s)\n", wifiOk ? "OK" : (WiFi.status() == WL_NO_SSID_AVAIL ? "NO_SSID" : "FAILED"), WiFi.localIP().toString().c_str());
    Serial.printf("  WebServer:   %s\n", (wifiOk && littleFsOk) ? "RUNNING" : "SKIPPED");
    Serial.printf("  MQTT Setup:  %s\n", mqttSetupOk ? "OK" : "FAILED"); // mqttSetupOk é sempre true, mas pode ser mais granular
    Serial.printf("  Sensors:     %s\n", sensorsOk ? "OK" : "FAILED");
    Serial.printf("  Actuators:   %s\n", actuatorsOk ? "OK" : "FAILED");
    Serial.printf("  Time Service:%s (Synced: %s)\n", timeService.isInitialized() ? "CONFIGURED" : "NOT_CONFIG", timeOk && timeService.getCurrentTime( *(new struct tm) ) ? "YES" : "NO/PENDING");
    Serial.printf("  MQTT Task:   %s\n", mqttTaskOk ? "RUNNING" : "NOT_RUNNING/SKIPPED");
    Serial.printf("  Sensor Task: %s\n", sensorTaskOk ? "RUNNING" : "NOT_RUNNING/SKIPPED");
    Serial.printf("  Actuator Tsk:%s\n", actuatorTasksOk ? "RUNNING" : "NOT_RUNNING/SKIPPED");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());

    if (displayOk) {
        bool systemDegraded = !littleFsOk || !sensorsOk || !actuatorsOk || !wifiOk || !timeOk || !mqttTaskOk || !sensorTaskOk || !actuatorTasksOk;
        // Não mostrar "Degraded" se estiver em modo de pareamento BLE, pois é um estado intencional
        if (digitalRead(PAIRING_BUTTON_PIN) == LOW && pServer != nullptr && pServer->getConnectedCount() == 0) { // Assumindo que pServer é setado em activatePairingMode
            // Já tratado em activatePairingMode
        } else if (systemDegraded) {
             displayMgr.printLine(0, "System Started");
             displayMgr.printLine(1, wifiOk ? WiFi.localIP().toString().c_str() : "Degraded Mode");
        } else if (wifiOk && littleFsOk) { // Se tudo OK e web server rodando
             displayMgr.printLine(0, "System OK"); // SensorManager irá atualizar com dados
             // displayMgr.printLine(1, WiFi.localIP().toString().c_str()); // Já feito em WebServer init
        }
    }
}

void loop() {
    // Verifica o botão de pareamento
    static unsigned long lastButtonCheck = 0;
    static bool buttonWasPressed = false;
    if (millis() - lastButtonCheck > 100) { // Verifica a cada 100ms
        bool buttonIsPressed = (digitalRead(PAIRING_BUTTON_PIN) == LOW);
        if (buttonIsPressed && !buttonWasPressed) {
            // Botão acabou de ser pressionado
            Serial.println("Pairing button pressed. Activating BLE pairing mode.");
            activatePairingMode();
        }
        buttonWasPressed = buttonIsPressed;
        lastButtonCheck = millis();
    }


    // Enviar eventos SSE periodicamente se o servidor web estiver ativo e WiFi conectado
    static unsigned long lastSseSendTime = 0;
    if (wifiOk && littleFsOk && (millis() - lastSseSendTime > 2000)) { // Envia a cada 2 segundos
        // WebServerManager deve verificar internamente se os managers de dados estão OK
        webServerManager.sendSensorUpdateEvent();
        webServerManager.sendStatusUpdateEvent();
        lastSseSendTime = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Reduz a carga da CPU no loop principal
}