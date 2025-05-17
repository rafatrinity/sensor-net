// src/network/webServerManager.hpp
#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "sensors/sensorManager.hpp"
#include "data/targetDataManager.hpp"
#include "actuators/actuatorManager.hpp"
#include "data/dataHistoryManager.hpp" // << NOVO INCLUDE
#include "data/historicDataPoint.hpp" // << NOVO INCLUDE

namespace GrowController {

class WebServerManager {
public:
    WebServerManager(uint16_t port,
                     SensorManager* sensorMgr,
                     TargetDataManager* targetMgr,
                     ActuatorManager* actuatorMgr,
                     DataHistoryManager* historyMgr); // << NOVO PARÂMETRO
    ~WebServerManager();

    WebServerManager(const WebServerManager&) = delete;
    WebServerManager& operator=(const WebServerManager&) = delete;

    void begin();
    void sendSensorUpdateEvent(); // Envia dados atuais dos sensores via SSE
    void sendStatusUpdateEvent(); // Envia status dos atuadores e alvos via SSE

private:
    SensorManager* sensorManager_;
    TargetDataManager* targetDataManager_;
    ActuatorManager* actuatorManager_;
    DataHistoryManager* dataHistoryManager_; // << NOVO MEMBRO

    AsyncWebServer server_;
    AsyncEventSource events_; // Para Server-Sent Events (SSE)
    uint16_t port_;

    // Controle de frequência para eventos SSE (opcional, mas bom para evitar sobrecarga)
    unsigned long lastSensorEventTime_ = 0;
    unsigned long lastStatusEventTime_ = 0;
    const unsigned long eventIntervalMs_ = 2000; // Intervalo mínimo entre eventos SSE em ms
};

} // namespace GrowController

#endif // WEBSERVER_MANAGER_HPP