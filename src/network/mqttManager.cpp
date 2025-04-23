// src/network/mqttManager.cpp
#include "mqttManager.hpp"
#include "utils/freeRTOSMutex.hpp" // Assumindo que usaremos nosso wrapper
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// --- Constantes ---
const TickType_t MQTT_MUTEX_TIMEOUT = pdMS_TO_TICKS(200); // Timeout para operações MQTT
const TickType_t MQTT_LOOP_DELAY_MS = pdMS_TO_TICKS(500); // Intervalo do loop principal da tarefa
const TickType_t MQTT_RECONNECT_DELAY_MS = pdMS_TO_TICKS(5000); // Delay antes de tentar reconectar
const uint8_t MQTT_CONNECT_RETRIES = 3; // Tentativas antes de um delay maior

namespace GrowController {

// --- REMOVED STATIC VARIABLE DEFINITION ---
// MqttManager* MqttManager::s_instance = nullptr;

// --- Construtor / Destrutor ---

MqttManager::MqttManager(const MQTTConfig& config, TargetDataManager& targetMgr) :
    mqttConfig(config),
    targetDataManager(targetMgr),
    pubSubClient(wifiClient), // Inicializa PubSubClient com WiFiClient
    clientMutex(nullptr),     // Será criado no setup/initialize
    taskHandle(nullptr),
    isSetup(false)
{
    // Calcula e armazena os tópicos base e de controle para eficiência
    baseTopic = String(mqttConfig.roomTopic);
    controlTopic = baseTopic + "/control";
}

MqttManager::~MqttManager() {
    Serial.println("MqttManager: Destructor called.");
    // Parar a tarefa se estiver rodando
    if (taskHandle != nullptr) {
        Serial.println("MqttManager: Stopping MQTT task...");
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
    // Deletar o mutex se foi criado
    if (clientMutex != nullptr) {
        vSemaphoreDelete(clientMutex);
        clientMutex = nullptr;
    }
    Serial.println("MqttManager: Destroyed.");
}

// --- Setup / Inicialização ---

void MqttManager::setup() {
    if (isSetup) {
        Serial.println("MqttManager WARN: Already setup.");
        return;
    }
    Serial.println("MqttManager: Setting up...");

    // 1. Criar Mutex
    clientMutex = xSemaphoreCreateMutex();
    if (clientMutex == nullptr) {
        Serial.println("MqttManager ERROR: Failed to create client mutex!");
        return;
    }
    Serial.println("MqttManager: Mutex created.");

    // 2. Configurar Servidor e Porta
    pubSubClient.setServer(mqttConfig.server, mqttConfig.port);
    Serial.printf("MqttManager: Server set to %s:%d\n", mqttConfig.server, mqttConfig.port);

    // 3. Configurar Callback usando LAMBDA
    pubSubClient.setClient(wifiClient); // Garante que o client está setado antes do callback
    pubSubClient.setCallback([this](char* topic, unsigned char* payload, unsigned int length) {
        // O lambda captura 'this' e chama o método de membro diretamente
        this->messageCallback(topic, payload, length);
    });
    Serial.println("MqttManager: Callback set using lambda.");

    // --- REMOVED s_instance logic ---
    // s_instance = this; // Armazena 'this' na variável estática
    // pubSubClient.setCallback(staticCallback); // A função estática usará s_instance

    // 4. Marcar como configurado
    isSetup = true;
    Serial.println("MqttManager: Setup complete.");
}

// --- REMOVED STATIC CALLBACK ---
/*
void MqttManager::staticCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("MqttManager: staticCallback invoked.");
    if (s_instance != nullptr) {
        // Chama o método de membro da instância armazenada
        s_instance->messageCallback(topic, payload, length);
    } else {
        Serial.println("MqttManager ERROR: staticCallback invoked but s_instance is null!");
    }
}
*/

// --- Callback de Membro (Lógica Real) ---
// *** NOTE: Signature changed to use unsigned char* ***
void MqttManager::messageCallback(char* topic, unsigned char* payload, unsigned int length) {
    Serial.println("===================================");
    Serial.print("MqttManager: Message arrived on topic: ");
    Serial.println(topic);

    // Criar uma String para facilitar o manuseio (cuidado com alocação)
    String message;
    message.reserve(length); // Pre-aloca espaço
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i]; // Cast unsigned char to char
    }
    Serial.print("MqttManager: Raw Message: ");
    Serial.println(message);

    // Compara com o tópico de controle armazenado
    if (String(topic) == this->controlTopic) {
        Serial.println("MqttManager: Processing control message...");

        JsonDocument doc; // Usa alocação dinâmica padrão do ArduinoJson v7+

        DeserializationError error = deserializeJson(doc, message); // Pass message String or payload directly

        if (error) {
            Serial.print("MqttManager ERROR: JSON deserialization failed: ");
            Serial.println(error.c_str());
            return;
        }

        // Chama o método do TargetDataManager injetado (THREAD-SAFE)
        Serial.println("MqttManager: Deserialized OK, updating targets via TargetDataManager...");
        // Pass the JsonDocument content (usually as JsonVariant or specific type)
        bool updated = targetDataManager.updateTargetsFromJson(doc);
        if (updated) {
            Serial.println("MqttManager: Targets updated successfully by callback.");
        } else {
            Serial.println("MqttManager WARN: TargetDataManager reported no targets were updated from JSON.");
        }

    } else {
        Serial.println("MqttManager: Message ignored (topic mismatch).");
    }
     Serial.println("===================================");
}


// --- Publicação ---

bool MqttManager::publish(const char* subTopic, float value, bool retained) {
    // Converte float para string
    char messageBuffer[16]; // Ajustar tamanho se necessário
    snprintf(messageBuffer, sizeof(messageBuffer), "%.2f", value);
    return publish(subTopic, messageBuffer, retained);
}

bool MqttManager::publish(const char* subTopic, const char* payload, bool retained) {
    if (!isSetup) {
        Serial.println("MqttManager ERROR: Cannot publish, not setup.");
        return false;
    }
     if (clientMutex == nullptr) {
         Serial.println("MqttManager ERROR: Cannot publish, mutex is null.");
         return false;
     }

    bool success = false;
    if (xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
        if (pubSubClient.connected()) {
            String fullTopic = baseTopic + "/" + subTopic;
            if (pubSubClient.publish(fullTopic.c_str(), payload, retained)) {
                // Serial.printf("MqttManager: Published [%s]: %s\n", fullTopic.c_str(), payload); // Verbose log
                success = true;
            } else {
                Serial.print("MqttManager ERROR: Publish Failed! State: ");
                Serial.println(pubSubClient.state());
                Serial.printf("  Topic: %s, Payload: %s\n", fullTopic.c_str(), payload);
            }
        } else {
            Serial.print("MqttManager WARN: Cannot publish, client not connected. Topic: ");
            Serial.print(baseTopic); Serial.print("/"); Serial.println(subTopic);
        }
        xSemaphoreGive(clientMutex);
    } else {
        Serial.println("MqttManager WARN: Could not acquire mutex for publish.");
    }
    return success;
}

// --- Gerenciamento da Tarefa ---

void MqttManager::taskRunner(void *pvParameter) {
    Serial.println("MqttManager::taskRunner started.");
    MqttManager* instance = static_cast<MqttManager*>(pvParameter);
    if (instance) {
        instance->run(); // Chama o loop principal da instância
    } else {
        Serial.println("MqttManager ERROR: taskRunner received null parameter!");
    }
     Serial.println("MqttManager::taskRunner finishing. Deleting task.");
    vTaskDelete(nullptr); // Deleta a tarefa atual
}

// Loop principal da tarefa
void MqttManager::run() {
    if (!isSetup) {
        Serial.println("MqttManager ERROR: Cannot run task, setup not complete.");
        return;
    }

    Serial.println("MqttManager: Task run loop started.");
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            ensureConnection();

            if (pubSubClient.connected()) {
                 if (clientMutex && xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
                      if(pubSubClient.connected()) { // Double check connection inside mutex
                         if (!pubSubClient.loop()) {
                            Serial.println("MqttManager WARN: pubSubClient.loop() returned false. Possible disconnection.");
                         }
                      }
                      xSemaphoreGive(clientMutex);
                 } // else { Serial.println("MqttManager WARN: Failed acquire mutex for loop."); }
            }
        } else {
             if (pubSubClient.connected()) {
                 if (clientMutex && xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
                     Serial.println("MqttManager INFO: WiFi disconnected, disconnecting MQTT client.");
                     pubSubClient.disconnect();
                     xSemaphoreGive(clientMutex);
                 }
             }
        }
        vTaskDelay(MQTT_LOOP_DELAY_MS);
    }
}

// --- Lógica de Conexão ---
void MqttManager::ensureConnection() {
     if (!isSetup || clientMutex == nullptr) {
         return;
     }

    if (pubSubClient.connected()) {
        return;
    }

    if (xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
        if (!pubSubClient.connected()) { // Double-check inside mutex
            Serial.print("MqttManager: Attempting MQTT connection to ");
            Serial.print(mqttConfig.server); Serial.print(":"); Serial.print(mqttConfig.port);
            Serial.print(" as client '"); Serial.print(mqttConfig.clientId); Serial.println("'...");

            // A lambda callback foi configurada no setup()
            if (pubSubClient.connect(mqttConfig.clientId)) {
                Serial.println("MqttManager: Connection successful!");

                // Publish Presence
                String presenceTopic = "devices"; // Example topic
                // Publish the base topic (room ID) to the presence topic
                if (publish(presenceTopic.c_str(), baseTopic.c_str(), true)) { // Retained = true for presence
                    Serial.printf("MqttManager: Published presence to '%s': %s\n", presenceTopic.c_str(), baseTopic.c_str());
                } else {
                    Serial.println("MqttManager WARN: Failed to publish presence message after connect.");
                }

                // Subscribe to Control Topic
                 Serial.print("MqttManager: Subscribing to control topic: ");
                 Serial.println(controlTopic);
                if (pubSubClient.subscribe(controlTopic.c_str())) {
                    Serial.println("MqttManager: Subscription successful.");
                } else {
                    Serial.println("MqttManager WARN: Failed to subscribe to control topic!");
                }

            } else {
                Serial.print("MqttManager ERROR: Connection Failed, state= ");
                Serial.print(pubSubClient.state());
                Serial.println(". Retrying later.");
            }
        }
        xSemaphoreGive(clientMutex);
    } // else { Serial.println("MqttManager WARN: Could not acquire mutex for connection attempt."); }
}

} // namespace GrowController