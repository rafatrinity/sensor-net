/**
 * @file mqtt.cpp
 * @brief Handles MQTT client setup, connection management, publishing, and the main task loop.
 * 
 * Utilizes the PubSubClient library for MQTT communication and integrates with
 * FreeRTOS tasks and semaphores for managing concurrent network operations.
 */

 #include "mqtt.hpp"
 #include "config.hpp"      // For MQTTConfig and global definitions
 #include "mqttHandler.hpp" // For the mqttCallback function
 
 #include <Arduino.h>
 #include <WiFi.h>          // Required for WiFi status checks and the WiFiClient
 #include <PubSubClient.h>
 #include <freertos/FreeRTOS.h> // For task delay and semaphore types
 #include <freertos/task.h>
 #include <freertos/semphr.h>   // For SemaphoreHandle_t
 
 // --- Global MQTT Objects ---
 WiFiClient espClient;              // TCP client for MQTT
 PubSubClient mqttClient(espClient); // MQTT client instance (still global for now)
 extern SemaphoreHandle_t mqttMutex; // Extern declaration for the shared MQTT mutex
 SemaphoreHandle_t mqttMutex = NULL;
 
 // --- Module Static Variables ---
 /**
  * @brief Stores the room topic configured during setup.
  * Used by mqttCallback (via extern in mqttHandler.cpp - needs refactoring) 
  * and ensureMQTTConnection for constructing topics.
  * @warning This static variable approach for sharing with the callback is not ideal.
  */
 String staticRoomTopic = "";
 
 /**
  * @brief Configures the MQTT client with server details and the message callback.
  * Does not attempt to connect here; connection is handled by the manageMQTT task.
  * 
  * @param config Constant reference to the MQTT configuration struct.
  */
 void setupMQTT(const MQTTConfig& config) {
   mqttClient.setServer(config.server, config.port);
   mqttClient.setCallback(mqttCallback);
 
   // Store the room topic for later use (e.g., in ensureMQTTConnection for subscription)
   staticRoomTopic = String(config.roomTopic); 
   Serial.printf("MQTT: Configured server %s:%d, Room Topic: %s\n", config.server, config.port, staticRoomTopic.c_str());
 
   // Removed initial ensureMQTTConnection call - manageMQTT task will handle the first connection.
   // Removed initial subscription call - Moved into ensureMQTTConnection success path.
 }
 
 /**
  * @brief Ensures the MQTT client is connected to the broker if WiFi is available.
  * If disconnected, it attempts to reconnect *once* per call. 
  * Intended to be called periodically by the manageMQTT task loop.
  * Handles acquiring the mqttMutex for connection and subscription operations.
  * 
  * @param config Constant reference to the MQTT configuration struct.
  * @note This function no longer contains an internal retry loop. Retries rely on repeated calls from manageMQTT.
  */
 void ensureMQTTConnection(const MQTTConfig& config) {
     // Only proceed if WiFi is connected
     if (WiFi.status() != WL_CONNECTED) {
         // Optionally log that MQTT connection is paused due to WiFi
         // Serial.println("MQTT: WiFi not connected, skipping connection check.");
         return; 
     }
 
     // Check connection status *before* taking mutex for efficiency
     if (!mqttClient.connected()) { 
          // Attempt to take the mutex. If another task holds it (e.g., publishing), wait.
          if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Wait up to 100ms for mutex
             // Double-check connection status *after* acquiring the mutex, 
             // in case another task connected successfully while we waited.
             if (!mqttClient.connected()) { 
                 Serial.print(F("MQTT: Attempting connection to "));
                 Serial.print(config.server);
                 Serial.print(F(":"));
                 Serial.print(config.port);
                 Serial.print(F(" as client '"));
                 Serial.print(config.clientId);
                 Serial.println(F("'..."));
 
                 // Attempt to connect
                 if (mqttClient.connect(config.clientId)) { 
                     Serial.println(F("MQTT: Connection successful!"));
 
                     // --- Actions on Successful Connection ---
                     
                     // 1. Publish the 'devices' presence message 
                     String devicesTopic = "devices"; // Or make this configurable if needed
                     if (mqttClient.publish(devicesTopic.c_str(), config.roomTopic)) { 
                         Serial.print(F("MQTT: Published presence to '"));
                         Serial.print(devicesTopic);
                         Serial.print(F("': "));
                         Serial.println(config.roomTopic);
                     } else {
                         Serial.println(F("MQTT WARN: Failed to publish presence message."));
                     }
 
                     // 2. Subscribe/Re-subscribe to the control topic
                     // Use the staticRoomTopic which was set during setupMQTT
                     String controlTopic = staticRoomTopic + "/control"; 
                     if(mqttClient.subscribe(controlTopic.c_str())){
                         Serial.print(F("MQTT: Successfully subscribed to: ")); 
                         Serial.println(controlTopic);
                     } else {
                         Serial.print(F("MQTT WARN: Failed to subscribe to: ")); 
                         Serial.println(controlTopic);
                         // Consider retrying subscription later?
                     }
                     // --- End Actions on Success ---
 
                 } else {
                     // Connection failed
                     Serial.print(F("MQTT: Connection Failed, state= "));
                     Serial.print(mqttClient.state());
                     Serial.println(F(". Will retry on next cycle."));
                     // No vTaskDelay here - rely on manageMQTT's loop delay
                 }
             } else {
                  // This case is unlikely if only manageMQTT calls connect, but handles race conditions
                  // Serial.println(F("MQTT: Already connected (checked after mutex acquisition)."));
             }
             // Release the mutex regardless of connection success/failure inside this block
             xSemaphoreGive(mqttMutex); 
          } else {
              Serial.println(F("MQTT WARN: Could not acquire MQTT mutex for connection attempt. Will retry."));
              // If mutex can't be acquired, another MQTT operation is likely in progress.
          }
     }
     // If already connected, the function does nothing.
 }
 
 /**
  * @brief Publishes a float value to a specified MQTT topic.
  * Acquires the MQTT mutex before publishing to ensure thread safety.
  * Includes a delay after publishing (consider reviewing/removing this).
  * 
  * @param topic The MQTT topic to publish to.
  * @param value The float value to publish (will be converted to string).
  * 
  * @warning Contains a vTaskDelay(2000) which might not be desirable. Publishing should ideally be non-blocking.
  */
 void publishMQTTMessage(const char* topic, float value) {
     // Convert float to string with 2 decimal places
     char messageBuffer[16]; // Adjust size if needed for larger numbers/precision
     snprintf(messageBuffer, sizeof(messageBuffer), "%.2f", value);
 
     // Acquire mutex - wait indefinitely as publishing is important
     if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
         // Check connection status *after* acquiring mutex
         if (mqttClient.connected()) {
             if (mqttClient.publish(topic, messageBuffer, false)) { // `false` for not retained
                 Serial.print(F("MQTT: Published [")); 
                 Serial.print(topic); 
                 Serial.print(F("]: "));
                 Serial.println(messageBuffer);
             } else {
                 Serial.print(F("MQTT ERROR: Publish Failed! State: ")); 
                 Serial.println(mqttClient.state());
                 Serial.print(F("  Topic: ")); Serial.println(topic);
                 Serial.print(F("  Message: ")); Serial.println(messageBuffer);
                 // Consider error handling: maybe flag for reconnection?
             }
         } else {
             Serial.print(F("MQTT WARN: Cannot publish, client not connected. Topic: "));
             Serial.println(topic);
         }
         // Release mutex
         xSemaphoreGive(mqttMutex);
     } else {
         // This should ideally not happen with portMAX_DELAY unless mutex is never given back elsewhere
         Serial.println(F("MQTT FATAL ERROR: Could not acquire mutex for publishMQTTMessage!"));
     }
 
     // TODO: Review this delay. Is it necessary? It blocks the calling task.
     vTaskDelay(pdMS_TO_TICKS(2000)); 
 }
 
 
 /**
  * @brief FreeRTOS task function to manage the MQTT connection and message loop.
  * Periodically calls ensureMQTTConnection to maintain the connection and
  * mqttClient.loop() to process incoming messages and keepalives.
  * 
  * @param parameter Pointer to the MQTTConfig struct.
  */
 void manageMQTT(void *parameter){
   // Cast parameter to the expected configuration type
   const MQTTConfig* mqttConfig = static_cast<const MQTTConfig*>(parameter);
    if (mqttConfig == nullptr) {
         Serial.println(F("MQTT FATAL ERROR: MQTTTask received NULL config!"));
         vTaskDelete(NULL); // Abort the task
         return;
     }
 
   // Configure the MQTT client (server, port, callback)
   setupMQTT(*mqttConfig); 
 
   // Delay slightly after setup before starting the main loop
   vTaskDelay(pdMS_TO_TICKS(1000)); 
 
   // Main task loop delay - determines check frequency and retry delay
   const TickType_t loopDelay = pdMS_TO_TICKS(1000); // Check/retry every 1 second 
 
   Serial.println(F("MQTT: Management task started."));
 
   while (true) {
       // Check WiFi status before attempting MQTT operations
       if (WiFi.status() == WL_CONNECTED) {
           
           // 1. Ensure connection (attempts reconnection if needed, once per loop)
           ensureMQTTConnection(*mqttConfig); 
 
           // 2. Process incoming messages and keepalive pings if connected
           //    Use a short timeout for the loop mutex to avoid blocking ensureMQTTConnection for too long
           if (mqttClient.connected()) { 
               if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                   // Check connection again *inside* mutex, as state might change
                   if(mqttClient.connected()) { 
                      // Process MQTT background tasks (keepalive, incoming messages)
                      if (!mqttClient.loop()) {
                         // mqttClient.loop() returns false if disconnected during processing
                         Serial.println(F("MQTT WARN: mqttClient.loop() indicated disconnection."));
                         // ensureMQTTConnection will handle reconnection on the next iteration
                      }
                   }
                   xSemaphoreGive(mqttMutex);
               } else {
                   // Avoid flooding serial if mutex is frequently busy
                   // Serial.println(F("MQTTTask WARN: Failed acquire mutex for loop.")); 
               }
           } 
           // If not connected, ensureMQTTConnection handles the retry logic initiated above.
           
       } else {
           // Optional: Log if WiFi is disconnected
           // Serial.println(F("MQTT: WiFi disconnected, pausing MQTT activity."));
 
           // Optional: Explicitly disconnect MQTT client if WiFi drops? Helps free resources faster.
           if (mqttClient.connected()) {
              if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                 mqttClient.disconnect(); // Disconnect TCP socket
                 xSemaphoreGive(mqttMutex);
                 Serial.println(F("MQTT: Client disconnected due to WiFi drop."));
              }
           }
       }
       
       // Wait before the next cycle
       vTaskDelay(loopDelay);
   }
 }