// src/actuators/actuatorManager.hpp
#ifndef ACTUATOR_MANAGER_HPP
#define ACTUATOR_MANAGER_HPP

#include "config.hpp"               // For GPIOControlConfig
#include "data/targetDataManager.hpp" // For TargetDataManager
#include "sensors/sensorManager.hpp"  // For SensorManager
#include "utils/timeService.hpp"    // For TimeService
#include "utils/freeRTOSMutex.hpp"  // For FreeRTOSMutex (if needed for shared state, though not explicitly used in this header)
#include <time.h>                   // For struct tm
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace GrowController {

// Forward declaration for TimeService, if preferred over direct include in some contexts,
// but direct include is generally fine for member types.
// class TimeService;

/**
 * @brief Manages the control of actuators like lights and humidifiers.
 * 
 * This class is responsible for initializing and controlling actuators based on
 * target values (e.g., from TargetDataManager) and current conditions (e.g., time from TimeService,
 * sensor readings from SensorManager). It uses FreeRTOS tasks for continuous control loops.
 */
class ActuatorManager {
public:
    /**
     * @brief Constructs a new ActuatorManager instance.
     * 
     * @param config Reference to the GPIOControlConfig containing pin definitions for actuators.
     * @param targetMgr Reference to the TargetDataManager to get target settings (e.g., light on/off times, target humidity).
     * @param sensorMgr Reference to the SensorManager to get current sensor readings (e.g., humidity).
     * @param timeSvc Reference to the TimeService to get the current time for time-based control (e.g., lights).
     */
    ActuatorManager(const GPIOControlConfig& config,
                    TargetDataManager& targetMgr,
                    SensorManager& sensorMgr,
                    TimeService& timeSvc);

    /**
     * @brief Destroys the ActuatorManager instance.
     * Ensures that control tasks are stopped and resources are cleaned up.
     */
    ~ActuatorManager();

    // Delete copy constructor and assignment operator to prevent copying.
    ActuatorManager(const ActuatorManager&) = delete;
    ActuatorManager& operator=(const ActuatorManager&) = delete;

    /**
     * @brief Initializes the ActuatorManager.
     * Sets up GPIO pins for actuators.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize();

    /**
     * @brief Starts the FreeRTOS tasks for controlling the light and humidifier.
     * 
     * @param lightTaskPriority Priority for the light control task.
     * @param humidityTaskPriority Priority for the humidity control task.
     * @param stackSize Stack size for each control task.
     * @return true if both tasks were started successfully, false otherwise.
     */
    bool startControlTasks(UBaseType_t lightTaskPriority = 1,
                           UBaseType_t humidityTaskPriority = 1,
                           uint32_t stackSize = 2560);

    /**
     * @brief Checks if the ActuatorManager has been successfully initialized.
     * @return true if initialized, false otherwise.
     */
    bool isInitialized() const;

    /**
     * @brief Checks if the light relay is currently considered ON.
     * This reflects the last commanded state, not necessarily a direct hardware read.
     * @return true if the light relay is ON, false otherwise.
     */
    bool isLightRelayOn() const;

    /**
     * @brief Checks if the humidifier relay is currently considered ON.
     * This reflects the last commanded state, not necessarily a direct hardware read.
     * @return true if the humidifier relay is ON, false otherwise.
     */
    bool isHumidifierRelayOn() const;

private:
    /**
     * @brief Checks current time against target light schedule and controls the light relay.
     * @param lightOn The target time for lights to turn ON.
     * @param lightOff The target time for lights to turn OFF.
     * @param lightPin The GPIO pin connected to the light relay.
     */
    void checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int lightPin);

    /**
     * @brief Checks current humidity against target humidity and controls the humidifier relay.
     * @param currentHumidity The current humidity reading from sensors.
     * @param targetHumidity The desired target humidity.
     * @param humidityPin The GPIO pin connected to the humidifier relay.
     */
    void checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin);

    /**
     * @brief Main loop for the light control task.
     * Periodically calls checkAndControlLight.
     */
    void runLightControlTask();

    /**
     * @brief Main loop for the humidity control task.
     * Periodically calls checkAndControlHumidity.
     */
    void runHumidityControlTask();

    /**
     * @brief Static wrapper function for the light control FreeRTOS task.
     * @param pvParameters Pointer to the ActuatorManager instance.
     */
    static void lightControlTaskWrapper(void* pvParameters);

    /**
     * @brief Static wrapper function for the humidity control FreeRTOS task.
     * @param pvParameters Pointer to the ActuatorManager instance.
     */
    static void humidityControlTaskWrapper(void* pvParameters);

    const GPIOControlConfig& gpioConfig;      ///< Configuration for GPIO pins.
    TargetDataManager& targetDataManager;   ///< Provides target values for control.
    SensorManager& sensorManager;           ///< Provides current sensor readings.
    TimeService& timeService;               ///< Provides current time.
    
    int lastLightState;                     ///< Last known state of the light relay (HIGH/LOW or -1 if unknown).
    TaskHandle_t lightTaskHandle;           ///< Handle for the light control task.
    TaskHandle_t humidityTaskHandle;        ///< Handle for the humidity control task.
    bool initialized;                       ///< Flag indicating if the manager is initialized.

    // Consider making these configurable or constants in a config file/namespace
    static const TickType_t LIGHT_CHECK_INTERVAL_MS;    ///< Interval for checking light status.
    static const TickType_t HUMIDITY_CHECK_INTERVAL_MS; ///< Interval for checking humidity status.
};

} // namespace GrowController

#endif // ACTUATOR_MANAGER_HPP
