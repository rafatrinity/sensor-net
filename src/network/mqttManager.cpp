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

    // 4. Marcar como configurado
    isSetup = true;
    Serial.println("MqttManager: Setup complete.");
}

// --- Callback de Membro (Lógica Real) ---
void MqttManager::messageCallback(char* topic, unsigned char* payload, unsigned int length) {
    Serial.println("===================================");
    Serial.print("MqttManager: Message arrived on topic: ");
    Serial.println(topic);

    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("MqttManager: Raw Message: ");
    Serial.println(message);

    if (String(topic) == this->controlTopic) {
        Serial.println("MqttManager: Processing control message...");
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            Serial.print("MqttManager ERROR: JSON deserialization failed: ");
            Serial.println(error.c_str());
            return;
        }
        Serial.println("MqttManager: Deserialized OK, updating targets via TargetDataManager...");
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
// Função privada que não bloqueia o mutex, assume que já está bloqueado
bool MqttManager::_publish_nolock(const char* fullTopic, const char* payload, bool retained) {
    if (pubSubClient.connected()) {
        if (pubSubClient.publish(fullTopic, payload, retained)) {
            // Serial.printf("MqttManager (nolock): Published [%s]: %s\n", fullTopic, payload); // Log opcional
            return true;
        } else {
            Serial.print("MqttManager ERROR (nolock): Publish Failed! State: ");
            Serial.println(pubSubClient.state());
            Serial.printf("  Topic: %s, Payload: %s\n", fullTopic, payload);
            return false;
        }
    } else {
        Serial.print("MqttManager WARN (nolock): Cannot publish, client not connected. Topic: ");
        Serial.println(fullTopic);
        return false;
    }
}

bool MqttManager::publish(const char* subTopic, float value, bool retained) {
    char messageBuffer[16];
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
        // A função publish pública agora chama _publish_nolock,
        // então não há necessidade de verificar pubSubClient.connected() aqui novamente,
        // pois _publish_nolock já faz isso.
        String fullTopic = baseTopic + "/" + subTopic;
        success = _publish_nolock(fullTopic.c_str(), payload, retained); // Usa a versão _nolock
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
        instance->run();
    } else {
        Serial.println("MqttManager ERROR: taskRunner received null parameter!");
    }
     Serial.println("MqttManager::taskRunner finishing. Deleting task.");
    vTaskDelete(nullptr);
}

void MqttManager::run() {
    if (!isSetup) {
        Serial.println("MqttManager ERROR: Cannot run task, setup not complete.");
        return;
    }

    Serial.println("MqttManager: Task run loop started.");
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            ensureConnection(); // Tenta conectar/reconectar se necessário

            // Loop do cliente MQTT (se conectado)
            if (pubSubClient.connected()) {
                 if (clientMutex && xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
                      if(pubSubClient.connected()) { // Double check connection inside mutex
                         if (!pubSubClient.loop()) { // Processa mensagens recebidas e mantém a conexão
                            Serial.println("MqttManager WARN: pubSubClient.loop() returned false. Possible disconnection.");
                         }
                      }
                      xSemaphoreGive(clientMutex);
                 } // else { Serial.println("MqttManager WARN: Failed acquire mutex for loop."); }
            }
        } else {
             // Se o WiFi desconectar, desconecta o cliente MQTT também
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

    // Se já estiver conectado, não faz nada
    if (pubSubClient.connected()) {
        return;
    }

    // Tenta adquirir o mutex para a operação de conexão
    if (xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
        // Double-check a conexão DENTRO do mutex, caso outra tarefa tenha conectado enquanto esperava
        if (!pubSubClient.connected()) {
            Serial.print("MqttManager: Attempting MQTT connection to ");
            Serial.print(mqttConfig.server); Serial.print(":"); Serial.print(mqttConfig.port);
            Serial.print(" as client '"); Serial.print(mqttConfig.clientId); Serial.println("'...");

            // Tenta conectar
            if (pubSubClient.connect(mqttConfig.clientId)) {
                Serial.println("MqttManager: Connection successful!");

                // Publish Presence (usando _publish_nolock pois o mutex já está bloqueado)
                String presenceSubTopic = "devices"; // Subtópico para presença
                String fullPresenceTopic = baseTopic + "/" + presenceSubTopic; // Tópico completo: <roomTopic>/devices
                
                // O payload da mensagem de presença será o ID do cômodo (baseTopic)
                if (_publish_nolock(fullPresenceTopic.c_str(), baseTopic.c_str(), true)) { // Retained = true
                    Serial.printf("MqttManager: Published presence to '%s': %s\n", fullPresenceTopic.c_str(), baseTopic.c_str());
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
                // Não há delay aqui, o loop run() já tem um vTaskDelay
            }
        }
        xSemaphoreGive(clientMutex); // Libera o mutex
    } // else { Serial.println("MqttManager WARN: Could not acquire mutex for connection attempt."); }
}

} // namespace GrowController