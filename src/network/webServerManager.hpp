// src/network/webServerManager.hpp
#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "sensors/sensorManager.hpp"
#include "data/targetDataManager.hpp"
#include "actuators/actuatorManager.hpp"

namespace GrowController {

class WebServerManager {
public:
    WebServerManager(uint16_t port, SensorManager* sensorMgr, TargetDataManager* targetMgr, ActuatorManager* actuatorMgr);
    ~WebServerManager();

    WebServerManager(const WebServerManager&) = delete;
    WebServerManager& operator=(const WebServerManager&) = delete;

    void begin();

    void sendSensorUpdateEvent();
    void sendStatusUpdateEvent();

private:
    SensorManager* sensorManager_;
    TargetDataManager* targetDataManager_;
    ActuatorManager* actuatorManager_;

    AsyncWebServer server_;
    AsyncEventSource events_;

    uint16_t port_;

    unsigned long lastSensorEventTime_ = 0;
    unsigned long lastStatusEventTime_ = 0;
    const unsigned long eventInterval_ = 2000;
};

} // namespace GrowController

#endif // WEBSERVER_MANAGER_HPP