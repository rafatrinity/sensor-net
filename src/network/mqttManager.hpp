#ifndef MQTT_MANAGER_HPP
#define MQTT_MANAGER_HPP

#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.hpp"
#include "targetDataManager.hpp" // Dependência para o callback
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace GrowController {

class MqttManager {
public:
    // Passa dependências (config, target manager) pelo construtor (Injeção de Dependência)
    MqttManager(const MQTTConfig& config, TargetDataManager& targetMgr);
    ~MqttManager(); // Limpa recursos se necessário

    /**
     * @brief Inicializa o cliente MQTT (servidor, porta, callback).
     * Não conecta ainda.
     */
    void setup();

    /**
     * @brief Publica uma mensagem float em um tópico específico.
     * Thread-safe. Assume que o tópico base (room) já está configurado.
     * @param subTopic O subtópico (ex: "sensors", "temperature"). Será concatenado com o roomTopic.
     * @param value O valor float a ser publicado.
     * @param retained Se a mensagem deve ser retida pelo broker.
     * @return true Se a publicação foi enfileirada com sucesso (não garante entrega).
     * @return false Se o cliente não está conectado ou houve erro ao publicar.
     */
    bool publish(const char* subTopic, float value, bool retained = false);
     /**
      * @brief Publica uma mensagem string em um tópico específico.
      * Thread-safe. Assume que o tópico base (room) já está configurado.
      * @param subTopic O subtópico (ex: "status", "alerts"). Será concatenado com o roomTopic.
      * @param payload A string a ser publicada.
      * @param retained Se a mensagem deve ser retida pelo broker.
      * @return true Se a publicação foi enfileirada com sucesso.
      * @return false Se o cliente não está conectado ou houve erro ao publicar.
      */
     bool publish(const char* subTopic, const char* payload, bool retained = false);


    /**
     * @brief Função da tarefa FreeRTOS para gerenciar a conexão e o loop MQTT.
     * @param pvParameter Ponteiro para a instância de MqttManager.
     */
    static void taskRunner(void *pvParameter); // Função estática para xTaskCreate

private:
    /**
     * @brief Loop principal da tarefa, chamado por taskRunner.
     */
    void run();

    /**
     * @brief Garante que a conexão MQTT esteja ativa se o WiFi estiver conectado.
     * Tenta reconectar se necessário. Chamado por run().
     */
    void ensureConnection();

    /**
     * @brief Processa mensagens MQTT recebidas e mantém a conexão ativa.
     * Chamado por run().
     */
    void loop();

     /**
     * @brief Callback interno para mensagens MQTT recebidas.
     * Chamado pelo PubSubClient. Usa targetDataManager para atualizar alvos.
     */
    void messageCallback(char* topic, byte* payload, unsigned int length);
     // Função estática wrapper para o callback do PubSubClient
     static void staticCallback(char* topic, byte* payload, unsigned int length, void* object);

    const MQTTConfig& mqttConfig; // Referência à configuração
    TargetDataManager& targetDataManager; // Referência ao gerenciador de alvos
    WiFiClient wifiClient;
    PubSubClient pubSubClient;
    SemaphoreHandle_t clientMutex; // Mutex para proteger pubSubClient
    String controlTopic; // Tópico de controle completo (room/control)
    String baseTopic;    // Tópico base (room)
    TaskHandle_t taskHandle = NULL;
    bool isSetup = false;
};

} // namespace GrowController
#endif // MQTT_MANAGER_HPP