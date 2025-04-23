// src/actuators/actuatorManager.hpp
#ifndef ACTUATOR_MANAGER_HPP
#define ACTUATOR_MANAGER_HPP

#include "config.hpp"               // Para GPIOControlConfig
#include "data/targetDataManager.hpp" // Para TargetDataManager
#include "sensors/sensorManager.hpp"  // Para SensorManager (Dependência!)
#include "utils/timeService.hpp"    // <<< VERIFIQUE SE ESTE CAMINHO ESTÁ CORRETO
#include "utils/freeRTOSMutex.hpp"
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace GrowController {

// Forward declaration pode ser usada se o include falhar e TimeService for classe
// class TimeService;

class ActuatorManager {
public:
    ActuatorManager(const GPIOControlConfig& config,
                    TargetDataManager& targetMgr,
                    SensorManager& sensorMgr,
                    TimeService& timeSvc); // Assinatura OK

    ~ActuatorManager();

    ActuatorManager(const ActuatorManager&) = delete;
    ActuatorManager& operator=(const ActuatorManager&) = delete;

    bool initialize();
    bool startControlTasks(UBaseType_t lightTaskPriority = 1,
                           UBaseType_t humidityTaskPriority = 1,
                           uint32_t stackSize = 2560);
    bool isInitialized() const;

private:
    void checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int lightPin);
    void checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin);
    void runLightControlTask();
    void runHumidityControlTask();
    static void lightControlTaskWrapper(void* pvParameters);
    static void humidityControlTaskWrapper(void* pvParameters);

    const GPIOControlConfig& gpioConfig;
    TargetDataManager& targetDataManager;
    SensorManager& sensorManager;
    TimeService& timeService; // Membro OK
    int lastLightState = -1;
    TaskHandle_t lightTaskHandle = nullptr;
    TaskHandle_t humidityTaskHandle = nullptr;
    bool initialized = false;

    static const TickType_t LIGHT_CHECK_INTERVAL_MS;
    static const TickType_t HUMIDITY_CHECK_INTERVAL_MS;
};

} // namespace GrowController

#endif // ACTUATOR_MANAGER_HPP