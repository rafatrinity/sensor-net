#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> // Para JsonDocument e cia.
#include "sensors/sensorManager.hpp"
#include "data/targetDataManager.hpp"
#include "actuators/actuatorManager.hpp"

// Forward declaration para AsyncEventSource se não quiser incluir o cabeçalho aqui
// class AsyncEventSource;

namespace GrowController {

class WebServerManager {
public:
    WebServerManager(SensorManager* sensorMgr, TargetDataManager* targetMgr, ActuatorManager* actuatorMgr);
    ~WebServerManager();

    WebServerManager(const WebServerManager&) = delete;
    WebServerManager& operator=(const WebServerManager&) = delete;

    void begin(); // Configura rotas e inicia o servidor

    // Métodos para enviar eventos SSE
    void sendSensorUpdateEvent();
    void sendStatusUpdateEvent();

private:
    SensorManager* sensorManager_;
    TargetDataManager* targetDataManager_;
    ActuatorManager* actuatorManager_;

    AsyncWebServer server_;
    AsyncEventSource events_; // Para Server-Sent Events

    // Handlers de rota (podem ser lambdas diretamente no .cpp também)
    void handleRoot(AsyncWebServerRequest *request);
    void handleGetSensors(AsyncWebServerRequest *request);
    void handleGetStatus(AsyncWebServerRequest *request);
    void handlePostTargets(AsyncWebServerRequest *request, JsonVariant &json);
    void handleNotFound(AsyncWebServerRequest *request);

    // Variáveis para controlar a frequência dos eventos SSE (opcional, para evitar spam)
    unsigned long lastSensorEventTime_ = 0;
    unsigned long lastStatusEventTime_ = 0;
    const unsigned long eventInterval_ = 2000; // Intervalo mínimo em ms entre eventos SSE

    // Métodos auxiliares para obter o estado dos atuadores (a serem implementados no ActuatorManager)
    // bool getLightState();
    // bool getHumidifierState();
};

} // namespace GrowController

#endif // WEBSERVER_MANAGER_HPP