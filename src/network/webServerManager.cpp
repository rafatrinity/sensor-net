#include "webServerManager.hpp"
#include <LittleFS.h>
#include <Arduino.h> 
namespace GrowController {

WebServerManager::WebServerManager(uint16_t port, SensorManager* sensorMgr, TargetDataManager* targetMgr, ActuatorManager* actuatorMgr) :
    sensorManager_(sensorMgr),
    targetDataManager_(targetMgr),
    actuatorManager_(actuatorMgr),
    server_(port),
    events_("/events"),
    port_(port)
{
    if (!sensorManager_ || !targetDataManager_ || !actuatorManager_) {
        Serial.println("WebServerManager ERROR: Null manager pointer(s) passed to constructor!");
    }
}

WebServerManager::~WebServerManager() {
    server_.end();
    events_.close();
}

void WebServerManager::begin() {
    // LittleFS já deve ter sido inicializado no main.cpp antes de chamar este begin()
    // if (!LittleFS.begin()) {
    //     Serial.println("WebServerManager ERROR: LittleFS mount failed. Web server not started.");
    //     return;
    // }
    // Serial.println("WebServerManager: LittleFS presumed mounted.");

    server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server_.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!sensorManager_) {
            request->send(500, "application/json", "{\"error\":\"SensorManager not available\"}");
            return;
        }
        JsonDocument doc; // ArduinoJson v7+
        doc["temperature"] = sensorManager_->getTemperature();
        doc["airHumidity"] = sensorManager_->getHumidity();
        doc["soilHumidity"] = sensorManager_->getSoilHumidity();
        doc["vpd"] = sensorManager_->getVpd();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!targetDataManager_ || !actuatorManager_) {
            request->send(500, "application/json", "{\"error\":\"DataManager or ActuatorManager not available\"}");
            return;
        }
        JsonDocument doc; // ArduinoJson v7+

        JsonObject light = doc["light"].to<JsonObject>();
        light["isOn"] = actuatorManager_->isLightRelayOn(); // Usando o novo getter
        struct tm lightOn = targetDataManager_->getLightOnTime();
        struct tm lightOff = targetDataManager_->getLightOffTime();
        char timeBuf[6];
        sprintf(timeBuf, "%02d:%02d", lightOn.tm_hour, lightOn.tm_min);
        light["onTime"] = String(timeBuf);
        sprintf(timeBuf, "%02d:%02d", lightOff.tm_hour, lightOff.tm_min);
        light["offTime"] = String(timeBuf);

        JsonObject humidifier = doc["humidifier"].to<JsonObject>();
        humidifier["isOn"] = actuatorManager_->isHumidifierRelayOn(); // Usando o novo getter
        humidifier["targetAirHumidity"] = targetDataManager_->getTargetAirHumidity();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    // Handler para POST /api/targets
    server_.onRequestBody([this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (request->url() == "/api/targets" && request->method() == HTTP_POST) {
            if (index == 0) { // Primeira parte do corpo (ou corpo inteiro se pequeno)
                // Serial.printf("POST /api/targets body: %.*s\n", len, (const char*)data); // Debug
            }

            if (index + len == total) { // Corpo completo recebido
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, (const char*)data, len);

                if (error) {
                    Serial.print(F("deserializeJson() failed for /api/targets: "));
                    Serial.println(error.f_str());
                    request->send(400, "application/json", "{\"success\":false, \"message\":\"Invalid JSON format\"}");
                    return;
                }

                if (!targetDataManager_) {
                    request->send(500, "application/json", "{\"success\":false, \"message\":\"TargetDataManager not available\"}");
                    return;
                }

                // Passar o JsonDocument diretamente para o método do TargetDataManager
                bool success = targetDataManager_->updateTargetsFromJson(doc);

                if (success) {
                    request->send(200, "application/json", "{\"success\":true, \"message\":\"Targets updated successfully.\"}");
                    this->sendStatusUpdateEvent(); // Notificar clientes SSE sobre a mudança de status/alvo
                } else {
                    request->send(400, "application/json", "{\"success\":false, \"message\":\"Error updating targets or no valid data.\"}");
                }
            }
        }
    });


    events_.onConnect([this](AsyncEventSourceClient *client){
        if(client->lastId()){
            Serial.printf("SSE Client Reconnected. Last ID: %u\n", client->lastId());
        } else {
            Serial.println("SSE Client connected");
        }
        // Enviar estado inicial imediatamente após a conexão
        sendSensorUpdateEvent();
        sendStatusUpdateEvent();
    });
    server_.addHandler(&events_);

    server_.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Resource Not Found");
    });

    server_.begin();
    Serial.println("WebServerManager: HTTP server started.");
}

void WebServerManager::sendSensorUpdateEvent() {
    if (!sensorManager_ || events_.count() == 0) {
        return;
    }
    // Opcional: controle de frequência
    // if (millis() - lastSensorEventTime_ < eventInterval_ && lastSensorEventTime_ != 0) {
    //     return;
    // }

    JsonDocument doc; // ArduinoJson v7+
    doc["temperature"] = sensorManager_->getTemperature();
    doc["airHumidity"] = sensorManager_->getHumidity();
    doc["soilHumidity"] = sensorManager_->getSoilHumidity();
    doc["vpd"] = sensorManager_->getVpd();

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "sensor_update", millis());
    lastSensorEventTime_ = millis();
}

void WebServerManager::sendStatusUpdateEvent() {
    if (!targetDataManager_ || !actuatorManager_ || events_.count() == 0) {
        return;
    }
    // Opcional: controle de frequência
    // if (millis() - lastStatusEventTime_ < eventInterval_ && lastStatusEventTime_ != 0) {
    //     return;
    // }

    JsonDocument doc; // ArduinoJson v7+
    JsonObject light = doc["light"].to<JsonObject>();
    light["isOn"] = actuatorManager_->isLightRelayOn();
    struct tm lightOn = targetDataManager_->getLightOnTime();
    struct tm lightOff = targetDataManager_->getLightOffTime();
    char timeBuf[6];
    sprintf(timeBuf, "%02d:%02d", lightOn.tm_hour, lightOn.tm_min);
    light["onTime"] = String(timeBuf);
    sprintf(timeBuf, "%02d:%02d", lightOff.tm_hour, lightOff.tm_min);
    light["offTime"] = String(timeBuf);

    JsonObject humidifier = doc["humidifier"].to<JsonObject>();
    humidifier["isOn"] = actuatorManager_->isHumidifierRelayOn();
    humidifier["targetAirHumidity"] = targetDataManager_->getTargetAirHumidity();

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "status_update", millis());
    lastStatusEventTime_ = millis();
}

} // namespace GrowController