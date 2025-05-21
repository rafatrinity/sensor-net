// src/network/webServerManager.hpp
#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "sensors/sensorManager.hpp"
#include "data/targetDataManager.hpp"
#include "actuators/actuatorManager.hpp"
#include "data/dataHistoryManager.hpp"
#include "data/historicDataPoint.hpp"

namespace GrowController
{

  /**
   * @brief Manages the ESPAsyncWebServer, handling HTTP requests and Server-Sent Events (SSE).
   *
   * This class is responsible for initializing the web server, setting up routes
   * (though route setup is typically done in the .cpp file), and sending real-time
   * updates to connected clients via SSE.
   */
  class WebServerManager
  {
  public:
    /**
     * @brief Constructs a new WebServerManager instance.
     * 
     * @param port The network port on which the server will listen.
     * @param sensorMgr Pointer to the SensorManager for accessing sensor data.
     * @param targetMgr Pointer to the TargetDataManager for accessing target data.
     * @param actuatorMgr Pointer to the ActuatorManager for accessing actuator status.
     * @param historyMgr Pointer to the DataHistoryManager for accessing historical data.
     */
    WebServerManager(uint16_t port,
                     SensorManager *sensorMgr,
                     TargetDataManager *targetMgr,
                     ActuatorManager *actuatorMgr,
                     DataHistoryManager *historyMgr);

    /**
     * @brief Destroys the WebServerManager instance.
     */
    ~WebServerManager();

    WebServerManager(const WebServerManager &) = delete;
    WebServerManager &operator=(const WebServerManager &) = delete;

    /**
     * @brief Initializes the web server and starts listening for incoming connections.
     * This method should set up all HTTP routes and SSE endpoints.
     */
    void begin();

    /**
     * @brief Sends an SSE event with the current sensor readings.
     * This method is typically called periodically or when sensor data changes significantly.
     * It respects the `eventIntervalMs_` to avoid flooding clients.
     */
    void sendSensorUpdateEvent();

    /**
     * @brief Sends an SSE event with the current status of actuators and target values.
     * This method is typically called periodically or when status/target data changes.
     * It respects the `eventIntervalMs_` to avoid flooding clients.
     */
    void sendStatusUpdateEvent();

  private:
    SensorManager *sensorManager_;
    TargetDataManager *targetDataManager_;
    ActuatorManager *actuatorManager_;
    DataHistoryManager *dataHistoryManager_;

    AsyncWebServer server_;
    AsyncEventSource events_; // Para Server-Sent Events (SSE)
    uint16_t port_;

    // Controle de frequência para eventos SSE (opcional, mas bom para evitar sobrecarga)
    unsigned long lastSensorEventTime_ = 0;
    unsigned long lastStatusEventTime_ = 0;
    const unsigned long eventIntervalMs_ = 2000;
  };

} // namespace GrowController

#endif // WEBSERVER_MANAGER_HPP