/**
 * @file actuatorManager.cpp
 * @brief Manages the control of actuators like lights and humidity controllers.
 * 
 * Implements tasks and functions to control actuators based on target values
 * and sensor readings (obtained via sensorManager).
 */

 #include "actuatorManager.hpp"
 #include "config.hpp"           // For GPIOControlConfig
 #include "sensors/targets.hpp"  // For TargetValues (external global, TODO: Refactor)
 #include "utils/timeService.hpp"// For getCurrentTime
 #include "sensors/sensorManager.hpp" // For getCurrentHumidity
 
 #include <Arduino.h>
 #include <time.h>               // For struct tm
 #include <math.h>               // For isnan
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 
 // ==========================================================================
 // !!! WARNING: Global Dependency !!!
 // This module still relies on the global 'target' variable. This needs refactoring.
 // ==========================================================================
 extern TargetValues target; // TODO: Refactor away from global dependency.
 
 // --- Static Variables ---
 static int lastLightState = -1; // Internal state for light change logging
 
 /**
  * @brief Initializes the GPIO pins defined in the configuration as OUTPUTs.
  * Sets the initial state of the pins to LOW (off).
  * 
  * @param config Constant reference to the GPIO control configuration struct.
  */
 void initializeActuators(const GPIOControlConfig &config)
 {
     Serial.println(F("Initializing Actuators..."));
 
     // Initialize Light Control Pin
     Serial.printf("Initializing Light Control Pin: %d\n", config.lightControlPin);
     pinMode(config.lightControlPin, OUTPUT);
     digitalWrite(config.lightControlPin, LOW);
 
     // Initialize Humidity Control Pin
     Serial.printf("Initializing Humidity Control Pin: %d\n", config.humidityControlPin);
     pinMode(config.humidityControlPin, OUTPUT);
     digitalWrite(config.humidityControlPin, LOW);
 
     Serial.println(F("Actuators Initialized."));
 }
 
 /**
  * @brief Checks the current time against the target schedule and controls the light pin.
  * Calculates time in minutes since midnight for easy comparison, handling overnight schedules.
  * 
  * @param lightOn Constant reference to the tm struct representing the light ON time (HH:MM).
  * @param lightOff Constant reference to the tm struct representing the light OFF time (HH:MM).
  * @param gpioPin The GPIO pin number controlling the light.
  */
 void checkAndControlLight(const struct tm &lightOn, const struct tm &lightOff, int gpioPin)
 {
     struct tm timeinfo;
     if (!getCurrentTime(timeinfo))
     {
         Serial.println(F("ActuatorManager WARN: Failed to get current time for light control!"));
         // Optionally, turn off the light as a safe default?
         // digitalWrite(gpioPin, LOW); 
         return;
     }
 
     // Convert current time and target times to minutes since midnight
     int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
     int startMinutes = lightOn.tm_hour * 60 + lightOn.tm_min;
     int endMinutes = lightOff.tm_hour * 60 + lightOff.tm_min;
 
     bool shouldBeOn = false;
 
     if (startMinutes == endMinutes)
     {
         // If start and end times are the same, assume light should be OFF.
         shouldBeOn = false;
     }
     else if (startMinutes < endMinutes)
     {
         // Normal daytime schedule (e.g., 08:00 to 18:00)
         shouldBeOn = (nowMinutes >= startMinutes && nowMinutes < endMinutes);
     }
     else // startMinutes > endMinutes
     {
         // Overnight schedule (e.g., 20:00 to 06:00)
         shouldBeOn = (nowMinutes >= startMinutes || nowMinutes < endMinutes);
     }
 
     // Apply the desired state to the GPIO pin
     int desiredState = shouldBeOn ? HIGH : LOW;
     digitalWrite(gpioPin, desiredState);
 
     // Log only when the state changes
     if (desiredState != lastLightState)
     {
         Serial.printf("ActuatorManager: Light state changed to %s (Pin: %d, Schedule: %02d:%02d-%02d:%02d, Now: %02d:%02d)\n",
                       shouldBeOn ? "ON" : "OFF", 
                       gpioPin,
                       lightOn.tm_hour, lightOn.tm_min,
                       lightOff.tm_hour, lightOff.tm_min,
                       timeinfo.tm_hour, timeinfo.tm_min);
         lastLightState = desiredState;
     }
 }
 
 /**
  * @brief Checks the current humidity against the target and controls the humidifier pin.
  * Turns the humidifier ON if current humidity is below the target, OFF otherwise.
  * Handles invalid (NAN) sensor readings by turning the humidifier OFF.
  * 
  * @param currentHumidity The current air humidity reading (in %, or NAN if invalid).
  * @param targetHumidity The desired target air humidity (in %).
  * @param humidityPin The GPIO pin number controlling the humidifier.
  */
 void checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin)
 {
     // Validate inputs: Check for NAN and ensure target is positive
     if (isnan(currentHumidity) || isnan(targetHumidity) || targetHumidity <= 0)
     {
         // If data is invalid, ensure humidifier is OFF for safety.
         if (digitalRead(humidityPin) == HIGH)
         {
             digitalWrite(humidityPin, LOW);
             Serial.printf("ActuatorManager: Humidifier turned OFF due to invalid data (Current: %.1f, Target: %.1f, Pin: %d)\n",
                           currentHumidity, targetHumidity, humidityPin);
         }
         return;
     }
 
     // Simple ON/OFF logic: Turn ON if current humidity is less than target.
     bool shouldBeOn = (currentHumidity < targetHumidity);
 
     // Apply the new state only if it's different from the current state
     int currentState = digitalRead(humidityPin);
     int desiredState = shouldBeOn ? HIGH : LOW;
 
     if (desiredState != currentState)
     {
         digitalWrite(humidityPin, desiredState);
         Serial.printf("ActuatorManager: Humidifier state changed to %s (Current: %.1f%%, Target: %.1f%%, Pin: %d)\n",
                       shouldBeOn ? "ON" : "OFF",
                       currentHumidity, targetHumidity, humidityPin);
     }
 }
 
 /**
  * @brief FreeRTOS task function for periodically controlling the light.
  * Retrieves the light schedule from the global 'target' struct and calls
  * checkAndControlLight at regular intervals.
  * 
  * @param parameter Pointer to the GPIOControlConfig struct containing pin definitions.
  */
 void lightControlTask(void *parameter)
 {
     const GPIOControlConfig *gpioConfig = static_cast<const GPIOControlConfig *>(parameter);
     if (gpioConfig == nullptr)
     {
         Serial.println(F("ActuatorManager FATAL ERROR: Invalid GPIO config passed to lightControlTask!"));
         vTaskDelete(NULL); // Abort task
         return; // Should not be reached
     }
 
     const TickType_t checkInterval = pdMS_TO_TICKS(5000); // Check every 5 seconds
     Serial.println(F("Light Control Task started."));
 
     while (true)
     {
         // Note: Accessing global 'target' directly is not ideal.
         checkAndControlLight(target.lightOnTime, target.lightOffTime, gpioConfig->lightControlPin);
         vTaskDelay(checkInterval);
     }
 }
 
 /**
  * @brief FreeRTOS task function for periodically controlling humidity.
  * Retrieves the current humidity from the sensor manager's cache (getCurrentHumidity)
  * and the target humidity from the global 'target' struct. Calls
  * checkAndControlHumidity at regular intervals.
  * 
  * @param parameter Pointer to the GPIOControlConfig struct containing pin definitions.
  */
 void humidityControlTask(void *parameter)
 {
     const GPIOControlConfig *gpioConfig = static_cast<const GPIOControlConfig *>(parameter);
     if (gpioConfig == nullptr)
     {
         Serial.println(F("ActuatorManager FATAL ERROR: Invalid GPIO config passed to humidityControlTask!"));
         vTaskDelete(NULL); // Abort task
         return; // Should not be reached
     }
 
     const TickType_t checkInterval = pdMS_TO_TICKS(10000); // Check every 10 seconds
     Serial.println(F("Humidity Control Task started."));
 
     while (true)
     {
         // Get humidity from the centralized cache (thread-safe)
         float currentAirHumidity = getCurrentHumidity(); 
         
         // Get target humidity (still uses global 'target')
         float targetAirHumidity = target.airHumidity; 
 
         checkAndControlHumidity(currentAirHumidity, targetAirHumidity, gpioConfig->humidityControlPin);
 
         vTaskDelay(checkInterval);
     }
 }