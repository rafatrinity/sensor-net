#include "webServerManager.hpp"
#include <LittleFS.h>
#include <Arduino.h>    // Para String, etc.
#include "utils/logger.hpp"

namespace GrowController {

WebServerManager::WebServerManager(uint16_t port,
                                   SensorManager* sensorMgr,
                                   TargetDataManager* targetMgr,
                                   ActuatorManager* actuatorMgr,
                                   DataHistoryManager* historyMgr) :
    sensorManager_(sensorMgr),
    targetDataManager_(targetMgr),
    actuatorManager_(actuatorMgr),
    dataHistoryManager_(historyMgr), // << INICIALIZA NOVO MEMBRO
    server_(port),
    events_("/events"), // Define o endpoint para SSE
    port_(port)
{
    if (!sensorManager_ || !targetDataManager_ || !actuatorManager_ || !dataHistoryManager_) {
        // Logger::error("WebServerManager: Null manager pointer(s) passed to constructor!");
        // Lançar exceção ou marcar um estado de erro interno seria mais robusto.
    }
}

WebServerManager::~WebServerManager() {
    server_.end();
    events_.close(); // Fecha todos os clientes SSE
}

void WebServerManager::begin() {
    // LittleFS.begin() é chamado no main.cpp
    if (!LittleFS.exists("/index.html")) {
        Logger::warn("WebServerManager: index.html not found in LittleFS. Web UI might not work.");
    }

    server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server_.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!sensorManager_) {
            request->send(500, "application/json", "{\"error\":\"SensorManager not available\"}");
            return;
        }
        JsonDocument doc;
        float temp = sensorManager_->getTemperature();
        float airHum = sensorManager_->getHumidity();
        float soilHum = sensorManager_->getSoilHumidity();
        float vpd = sensorManager_->getVpd();

        if (!isnan(temp)) doc["temperature"] = temp;
        if (!isnan(airHum)) doc["airHumidity"] = airHum;
        if (!isnan(soilHum)) doc["soilHumidity"] = soilHum;
        if (!isnan(vpd)) doc["vpd"] = vpd;

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!targetDataManager_ || !actuatorManager_) {
            request->send(500, "application/json", "{\"error\":\"DataManager or ActuatorManager not available\"}");
            return;
        }
        JsonDocument doc;
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
        float targetAirHum = targetDataManager_->getTargetAirHumidity();
        if (!isnan(targetAirHum)) humidifier["targetAirHumidity"] = targetAirHum;

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    // >>> NOVO ENDPOINT: /api/history
    server_.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!dataHistoryManager_) {
            request->send(500, "application/json", "{\"error\":\"DataHistoryManager not available\"}");
            return;
        }

        std::vector<HistoricDataPoint> history = dataHistoryManager_->getAllDataPointsSorted();
        
        // Capacidade para JSON: N objetos * ( overhead_obj + N_campos * overhead_campo_valor) + overhead_array
        // Estimativa: 48 * (2 + 5 * (15+5)) + 2 = 48 * (2 + 100) + 2 ~ 4.9KB. Usar 8KB para segurança.
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        for (const auto& point : history) {
            JsonObject obj = array.add<JsonObject>();
            obj["timestamp"] = point.timestamp;
            
            if (!isnan(point.avgTemperature)) obj["avgTemperature"] = point.avgTemperature;
            if (!isnan(point.avgAirHumidity)) obj["avgAirHumidity"] = point.avgAirHumidity;
            if (!isnan(point.avgSoilHumidity)) obj["avgSoilHumidity"] = point.avgSoilHumidity;
            if (!isnan(point.avgVpd)) obj["avgVpd"] = point.avgVpd;
        }

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        // Logger::debug("Sending /api/history response, length: %d", jsonResponse.length());
        // if (jsonResponse.length() < 500) Logger::debug("History JSON: %s", jsonResponse.c_str());
        request->send(200, "application/json", jsonResponse);
    });

    // Handler para POST /api/targets com suporte a CORS
    server_.on("/api/targets", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(204);
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Max-Age", "86400");
        request->send(response);
    });

    server_.on("/api/targets", HTTP_POST, [](AsyncWebServerRequest *request) {
        // O handler real está em onRequestBody, mas precisamos configurar os headers CORS aqui
        if (!request->hasParam("body", true)) {
            AsyncWebServerResponse *response = request->beginResponse(200, "application/json", 
                "{\"success\":false, \"message\":\"No body received\"}");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
        }
    });

    // Handler para processamento do corpo da requisição
    server_.onRequestBody([this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (request->url() == "/api/targets" && request->method() == HTTP_POST) {
            static std::vector<uint8_t> requestBodyBuffer;
            
            // Proteger contra payloads muito grandes
            if (total > 1024) {
                AsyncWebServerResponse *response = request->beginResponse(413, "application/json", 
                    "{\"success\":false, \"message\":\"Payload too large\"}");
                response->addHeader("Access-Control-Allow-Origin", "*");
                request->send(response);
                return;
            }

            if (index == 0) {
                requestBodyBuffer.reserve(total);
                requestBodyBuffer.assign(data, data + len);
            } else {
                requestBodyBuffer.insert(requestBodyBuffer.end(), data, data + len);
            }

            if (index + len == total) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, requestBodyBuffer.data(), requestBodyBuffer.size());
                
                AsyncWebServerResponse *response;
                if (error) {
                    Logger::error("deserializeJson() failed for /api/targets: %s", error.c_str());
                    response = request->beginResponse(400, "application/json", 
                        "{\"success\":false, \"message\":\"Invalid JSON format\"}");
                    response->addHeader("Access-Control-Allow-Origin", "*");
                    request->send(response);
                    return;
                }

                if (!targetDataManager_) {
                    response = request->beginResponse(500, "application/json", 
                        "{\"success\":false, \"message\":\"TargetDataManager not available\"}");
                    response->addHeader("Access-Control-Allow-Origin", "*");
                    request->send(response);
                    return;
                }

                // Log para debug dos dados recebidos
                Logger::debug("Received targets update request: airHumidity=%.1f, lightOn=%s, lightOff=%s",
                    doc["targetAirHumidity"].as<float>(),
                    doc["lightOnTime"].as<const char*>(),
                    doc["lightOffTime"].as<const char*>());

                bool success = targetDataManager_->updateTargetsFromJson(doc);
                if (success) {
                    response = request->beginResponse(200, "application/json", 
                        "{\"success\":true, \"message\":\"Targets updated successfully.\"}");
                    response->addHeader("Access-Control-Allow-Origin", "*");
                    request->send(response);
                    this->sendStatusUpdateEvent();
                } else {
                    response = request->beginResponse(400, "application/json", 
                        "{\"success\":false, \"message\":\"Error updating targets or no valid data.\"}");
                    response->addHeader("Access-Control-Allow-Origin", "*");
                    request->send(response);
                }

                // Limpar buffer após processar
                requestBodyBuffer.clear();
                requestBodyBuffer.shrink_to_fit();
            }
        }
    });

    // Configuração do Server-Sent Events
    events_.onConnect([this](AsyncEventSourceClient *client){
        if(client->lastId()){
            // Logger::info("SSE Client Reconnected. Last ID: %u", client->lastId());
        } else {
            // Logger::info("SSE Client connected");
        }
        // Enviar estado inicial imediatamente após a conexão
        sendSensorUpdateEvent();
        sendStatusUpdateEvent();
    });
    server_.addHandler(&events_);

    server_.onNotFound([](AsyncWebServerRequest *request){
        // Logger::warn("WebServer: Resource Not Found - %s", request->url().c_str());
        if (request->url().startsWith("/api/")) {
             request->send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
        } else {
            // Para SPAs, pode ser necessário sempre servir index.html para rotas desconhecidas
            // request->send(LittleFS, "/index.html", "text/html");
            // Mas serveStatic com setDefaultFile já deve tratar isso.
            request->send(404, "text/plain", "Resource Not Found");
        }
    });

    server_.begin();
    Logger::info("WebServerManager: HTTP server started on port %d.", port_);
}

// Envio de eventos SSE
void WebServerManager::sendSensorUpdateEvent() {
    if (!sensorManager_ || events_.count() == 0) { // Só envia se houver clientes SSE conectados
        return;
    }
    // Controle de frequência (opcional, mas bom para não sobrecarregar)
    if (millis() - lastSensorEventTime_ < eventIntervalMs_ && lastSensorEventTime_ != 0) {
        return;
    }
    
    JsonDocument doc;
    float temp = sensorManager_->getTemperature();
    float airHum = sensorManager_->getHumidity();
    float soilHum = sensorManager_->getSoilHumidity();
    float vpd = sensorManager_->getVpd();
    if (!isnan(temp)) doc["temperature"] = temp;
    if (!isnan(airHum)) doc["airHumidity"] = airHum;
    if (!isnan(soilHum)) doc["soilHumidity"] = soilHum;
    if (!isnan(vpd)) doc["vpd"] = vpd;

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "sensor_update", millis());
    lastSensorEventTime_ = millis();
}

void WebServerManager::sendStatusUpdateEvent() {
    if (!targetDataManager_ || !actuatorManager_ || events_.count() == 0) {
        return;
    }
    if (millis() - lastStatusEventTime_ < eventIntervalMs_ && lastStatusEventTime_ != 0) {
        return;
    }

    JsonDocument doc;
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
    float targetAirHum = targetDataManager_->getTargetAirHumidity();
    if (isnan(targetAirHum)) humidifier["targetAirHumidity"] = nullptr;
    else humidifier["targetAirHumidity"] = targetAirHum;

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "status_update", millis());
    lastStatusEventTime_ = millis();
}

} // namespace GrowController