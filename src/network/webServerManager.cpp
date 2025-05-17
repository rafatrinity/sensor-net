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
        // Usar JsonDocument doc;
        JsonDocument doc;
        doc["temperature"] = serialized(String(sensorManager_->getTemperature(), 1));
        doc["airHumidity"] = serialized(String(sensorManager_->getHumidity(), 1));
        doc["soilHumidity"] = serialized(String(sensorManager_->getSoilHumidity(), 1));
        doc["vpd"] = serialized(String(sensorManager_->getVpd(), 2));

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
        humidifier["targetAirHumidity"] = serialized(String(targetDataManager_->getTargetAirHumidity(),1));

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
            // Formatar floats para consistência e tamanho. Usar String() cria objetos temporários.
            // Para melhor performance, poderia usar snprintf para um buffer e depois JsonObject::set().
            if (!isnan(point.avgTemperature)) obj["avgTemperature"] = serialized(String(point.avgTemperature, 1)); else obj["avgTemperature"] = nullptr;
            if (!isnan(point.avgAirHumidity)) obj["avgAirHumidity"] = serialized(String(point.avgAirHumidity, 1)); else obj["avgAirHumidity"] = nullptr;
            if (!isnan(point.avgSoilHumidity)) obj["avgSoilHumidity"] = serialized(String(point.avgSoilHumidity, 1)); else obj["avgSoilHumidity"] = nullptr;
            if (!isnan(point.avgVpd)) obj["avgVpd"] = serialized(String(point.avgVpd, 2)); else obj["avgVpd"] = nullptr;
        }

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        // Logger::debug("Sending /api/history response, length: %d", jsonResponse.length());
        // if (jsonResponse.length() < 500) Logger::debug("History JSON: %s", jsonResponse.c_str());
        request->send(200, "application/json", jsonResponse);
    });

    // Handler para POST /api/targets (atualizado para usar buffer estático)
    // Para evitar alocação dinâmica excessiva em onRequestBody, pode-se usar um buffer estático
    // ou uma abordagem de streaming se o payload for muito grande.
    // Aqui, vamos manter a lógica de acumular no `requestBodyBuffer` (que é static).
    server_.onRequestBody([this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (request->url() == "/api/targets" && request->method() == HTTP_POST) {
            static std::vector<uint8_t> requestBodyBuffer; // static para persistir entre chamadas parciais
            
            if (index == 0) { // Primeira parte do corpo
                requestBodyBuffer.assign(data, data + len); // Começa novo buffer
            } else { // Partes subsequentes
                requestBodyBuffer.insert(requestBodyBuffer.end(), data, data + len);
            }

            if (index + len == total) { // Corpo completo recebido
                // Logger::debug("POST /api/targets body (%d bytes): %.*s", total, total, (const char*)requestBodyBuffer.data());
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, requestBodyBuffer.data(), requestBodyBuffer.size());
                
                // Limpar o buffer estático para a próxima requisição
                // requestBodyBuffer.clear(); // Não precisa mais se assign é usado no início

                if (error) {
                    Logger::error("deserializeJson() failed for /api/targets: %s", error.c_str());
                    request->send(400, "application/json", "{\"success\":false, \"message\":\"Invalid JSON format\"}");
                    return;
                }

                if (!targetDataManager_) {
                    request->send(500, "application/json", "{\"success\":false, \"message\":\"TargetDataManager not available\"}");
                    return;
                }

                bool success = targetDataManager_->updateTargetsFromJson(doc);
                if (success) {
                    request->send(200, "application/json", "{\"success\":true, \"message\":\"Targets updated successfully.\"}");
                    this->sendStatusUpdateEvent(); // Notificar clientes SSE
                } else {
                    request->send(400, "application/json", "{\"success\":false, \"message\":\"Error updating targets or no valid data.\"}");
                }
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
    doc["temperature"] = serialized(String(sensorManager_->getTemperature(), 1));
    doc["airHumidity"] = serialized(String(sensorManager_->getHumidity(), 1));
    doc["soilHumidity"] = serialized(String(sensorManager_->getSoilHumidity(), 1));
    doc["vpd"] = serialized(String(sensorManager_->getVpd(), 2));

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
    humidifier["targetAirHumidity"] = serialized(String(targetDataManager_->getTargetAirHumidity(),1));

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "status_update", millis());
    lastStatusEventTime_ = millis();
}

} // namespace GrowController