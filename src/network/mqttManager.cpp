// src/network/mqttManager.cpp
#include "mqttManager.hpp"
#include "utils/freeRTOSMutex.hpp" // Assumindo que usaremos nosso wrapper
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h> // Para o callback

// --- Constantes ---
const TickType_t MQTT_MUTEX_TIMEOUT = pdMS_TO_TICKS(100); // Timeout para operações MQTT
const TickType_t MQTT_LOOP_DELAY_MS = pdMS_TO_TICKS(500); // Intervalo do loop principal da tarefa
const TickType_t MQTT_RECONNECT_DELAY_MS = pdMS_TO_TICKS(5000); // Delay antes de tentar reconectar
const uint8_t MQTT_CONNECT_RETRIES = 3; // Tentativas antes de um delay maior

namespace GrowController {

// --- Variáveis Estáticas (se absolutamente necessário, mas vamos evitar) ---
// Nenhuma por enquanto.

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

// Usaremos 'setup' como inicializador principal por enquanto
void MqttManager::setup() {
    if (isSetup) {
        Serial.println("MqttManager WARN: Already setup.");
        return;
    }
    Serial.println("MqttManager: Setting up...");

    // 1. Criar Mutex (Manual ou com Wrapper)
    // Usando manual por enquanto para compatibilidade com código existente
    clientMutex = xSemaphoreCreateMutex();
    if (clientMutex == nullptr) {
        Serial.println("MqttManager ERROR: Failed to create client mutex!");
        // Considerar lançar exceção ou ter um estado de erro
        return;
    }
    Serial.println("MqttManager: Mutex created.");

    // 2. Configurar Servidor e Porta
    pubSubClient.setServer(mqttConfig.server, mqttConfig.port);
    Serial.printf("MqttManager: Server set to %s:%d\n", mqttConfig.server, mqttConfig.port);

    // 3. Configurar Callback (IMPORTANTE: Usar wrapper estático e passar 'this')
    pubSubClient.setCallback(staticCallback); // Passa a função estática
    pubSubClient.setClient(wifiClient);      // Garante que o client está setado
    // O userdata será setado dinamicamente antes de conectar ou via um método da lib se disponível
    // Alternativa comum é um singleton ou uma variável estática global para a instância,
    // mas vamos tentar passar 'this' na conexão se a lib permitir, ou usar um workaround.
    // *** Workaround Comum: Variável Estática para a Instância ***
    // (Não ideal, mas funciona se setCallback não aceitar userdata diretamente)
    // static MqttManager* instancePtr = this; // <<< Cuidado com múltiplas instâncias!
    // pubSubClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    //    if(instancePtr) instancePtr->messageCallback(topic, payload, length);
    // });
    // *** Usando setCallback com userdata (melhor se a lib suportar implicitamente ou explicitamente) ***
    // A biblioteca PubSubClient padrão *não* tem um parâmetro userdata explícito em setCallback.
    // A chamada estática é a maneira mais comum de contornar isso.
    // Precisamos de uma forma de obter 'this' dentro de staticCallback.

    // >>> Solução: Usaremos uma variável estática *protegida* para a instância <<<<
    // (Isso limita a uma instância global, mas é um padrão comum em C com callbacks)
    s_instance = this; // Armazena 'this' na variável estática
    pubSubClient.setCallback(staticCallback); // A função estática usará s_instance
    Serial.println("MqttManager: Callback set.");


    // 4. Marcar como configurado
    isSetup = true;
    Serial.println("MqttManager: Setup complete.");
}

// --- Variável Estática para Instância (Workaround para Callback) ---
MqttManager* MqttManager::s_instance = nullptr;

// --- Callback Estático (Wrapper) ---
void MqttManager::staticCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("MqttManager: staticCallback invoked.");
    if (s_instance != nullptr) {
        // Chama o método de membro da instância armazenada
        s_instance->messageCallback(topic, payload, length);
    } else {
        Serial.println("MqttManager ERROR: staticCallback invoked but s_instance is null!");
    }
}

// --- Callback de Membro (Lógica Real) ---
void MqttManager::messageCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("===================================");
    Serial.print("MqttManager: Message arrived on topic: ");
    Serial.println(topic);

    // Criar uma String para facilitar o manuseio (cuidado com alocação)
    // Ou processar o payload diretamente como array de bytes
    String message;
    message.reserve(length); // Pre-aloca espaço
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("MqttManager: Raw Message: ");
    Serial.println(message);

    // Compara com o tópico de controle armazenado
    if (String(topic) == this->controlTopic) {
        Serial.println("MqttManager: Processing control message...");
        // Usar StaticJsonDocument para evitar alocação dinâmica excessiva no callback
        StaticJsonDocument<512> doc; // Aumentar tamanho se necessário
        DeserializationError error = deserializeJson(doc, message);

        if (error) {
            Serial.print("MqttManager ERROR: JSON deserialization failed: ");
            Serial.println(error.c_str());
            return;
        }

        // Chama o método do TargetDataManager injetado (THREAD-SAFE)
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

bool MqttManager::publish(const char* subTopic, float value, bool retained) {
    // Converte float para string
    char messageBuffer[16]; // Ajustar tamanho se necessário
    // Usar snprintf para formatação segura
    // Verificar o número de casas decimais desejado
    snprintf(messageBuffer, sizeof(messageBuffer), "%.2f", value);
    // Chama a versão string do publish
    return publish(subTopic, messageBuffer, retained);
}

bool MqttManager::publish(const char* subTopic, const char* payload, bool retained) {
    if (!isSetup) {
        Serial.println("MqttManager ERROR: Cannot publish, not setup.");
        return false;
    }
     if (clientMutex == nullptr) {
         Serial.println("MqttManager ERROR: Cannot publish, mutex is null.");
         return false; // Não deveria acontecer se setup foi chamado
     }

    bool success = false;
    // Tenta pegar o mutex para garantir exclusividade na operação MQTT
    if (xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
        if (pubSubClient.connected()) {
            // Constrói o tópico completo: baseTopic/subTopic
            String fullTopic = baseTopic + "/" + subTopic;
            // Realiza a publicação
            if (pubSubClient.publish(fullTopic.c_str(), payload, retained)) {
                // Serial.printf("MqttManager: Published [%s]: %s\n", fullTopic.c_str(), payload); // Log verboso
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
        // Libera o mutex
        xSemaphoreGive(clientMutex);
    } else {
        Serial.println("MqttManager WARN: Could not acquire mutex for publish.");
        // Falha ao pegar mutex também é considerado falha na publicação
    }
    return success;
}

// --- Gerenciamento da Tarefa ---

// Método estático chamado por xTaskCreate
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
        return; // Sai do método run, a tarefa será deletada pelo taskRunner
    }

    Serial.println("MqttManager: Task run loop started.");
    while (true) {
        // Só executa lógica MQTT se o WiFi estiver conectado
        if (WiFi.status() == WL_CONNECTED) {
            ensureConnection(); // Tenta conectar/reconectar se necessário

            // Processa o loop do cliente MQTT (keepalive, recebimento) se conectado
            if (pubSubClient.connected()) {
                 if (clientMutex && xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
                      // Double check connection inside mutex
                      if(pubSubClient.connected()) {
                         if (!pubSubClient.loop()) {
                            // loop() retorna false se houver erro ou desconexão
                            Serial.println("MqttManager WARN: pubSubClient.loop() returned false. Possible disconnection.");
                            // ensureConnection na próxima iteração tentará reconectar
                         }
                      }
                      xSemaphoreGive(clientMutex);
                 } else {
                      // Serial.println("MqttManager WARN: Failed acquire mutex for loop.");
                 }
            }
        } else {
             // Se o WiFi caiu, desconectar explicitamente o cliente MQTT?
             if (pubSubClient.connected()) {
                 if (clientMutex && xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
                     Serial.println("MqttManager INFO: WiFi disconnected, disconnecting MQTT client.");
                     pubSubClient.disconnect();
                     xSemaphoreGive(clientMutex);
                 }
             }
             // Serial.println("MqttManager: WiFi disconnected, skipping MQTT operations."); // Log opcional
        }

        // Pausa antes da próxima iteração
        vTaskDelay(MQTT_LOOP_DELAY_MS);
    }
     // Fim do while(true) - não deve ser alcançado
}

// --- Lógica de Conexão ---
void MqttManager::ensureConnection() {
     if (!isSetup || clientMutex == nullptr) {
         // Serial.println("MqttManager WARN: ensureConnection called before setup or without mutex.");
         return;
     }

    // Não tenta conectar se já estiver conectado
    // (Verificação fora do mutex para eficiência, mas checar de novo dentro)
    if (pubSubClient.connected()) {
        return;
    }

    // Tenta pegar o mutex para operações de conexão/subscrição
    if (xSemaphoreTake(clientMutex, MQTT_MUTEX_TIMEOUT) == pdTRUE) {
        // Double-check connection status *após* adquirir o mutex
        if (!pubSubClient.connected()) {
            Serial.print("MqttManager: Attempting MQTT connection to ");
            Serial.print(mqttConfig.server); Serial.print(":"); Serial.print(mqttConfig.port);
            Serial.print(" as client '"); Serial.print(mqttConfig.clientId); Serial.println("'...");

            // Tenta conectar
            // O PubSubClient não tem um método connect que aceita userdata diretamente
            // para o callback configurado anteriormente. O callback estático pegará a instância.
            if (pubSubClient.connect(mqttConfig.clientId)) {
                Serial.println("MqttManager: Connection successful!");

                // --- Ações Pós-Conexão ---

                // 1. Publicar Presença (Exemplo: "devices" -> "sala01")
                // Ajuste o tópico "devices" conforme sua necessidade
                String presenceTopic = "devices";
                if (publish(presenceTopic.c_str(), baseTopic.c_str())) { // Publica o tópico base como payload
                    Serial.printf("MqttManager: Published presence to '%s': %s\n", presenceTopic.c_str(), baseTopic.c_str());
                } else {
                    Serial.println("MqttManager WARN: Failed to publish presence message after connect.");
                }

                // 2. Subscrever ao Tópico de Controle
                 Serial.print("MqttManager: Subscribing to control topic: ");
                 Serial.println(controlTopic);
                if (pubSubClient.subscribe(controlTopic.c_str())) {
                    Serial.println("MqttManager: Subscription successful.");
                } else {
                    Serial.println("MqttManager WARN: Failed to subscribe to control topic!");
                    // Considerar tentar novamente? Ou desconectar?
                }
                // --- Fim Ações Pós-Conexão ---

            } else {
                // Falha na conexão
                Serial.print("MqttManager ERROR: Connection Failed, state= ");
                Serial.print(pubSubClient.state());
                Serial.println(". Retrying later.");
                // Não colocar delay longo aqui, o loop principal da tarefa já tem delay.
            }
        } // else { Serial.println("MqttManager DEBUG: Already connected (checked inside mutex)."); }

        // Libera o mutex independentemente do resultado
        xSemaphoreGive(clientMutex);

    } else {
         Serial.println("MqttManager WARN: Could not acquire mutex for connection attempt.");
    }
}


} // namespace GrowController