#include "webServerManager.hpp"
#include <LittleFS.h>
#include <Arduino.h> // Para Serial

namespace GrowController {

WebServerManager::WebServerManager(SensorManager* sensorMgr, TargetDataManager* targetMgr, ActuatorManager* actuatorMgr) :
    sensorManager_(sensorMgr),
    targetDataManager_(targetMgr),
    actuatorManager_(actuatorMgr),
    server_(80), // Servidor na porta 80
    events_("/events") // Eventos SSE no endpoint /events
{
    if (!sensorManager_ || !targetDataManager_ || !actuatorManager_) {
        Serial.println("WebServerManager ERROR: Null manager pointer(s) passed to constructor!");
        // Lidar com erro crítico, talvez impedir o begin()?
    }
}

WebServerManager::~WebServerManager() {
    server_.end();
}

// --- Métodos auxiliares para obter estado dos atuadores ---
// Estes são exemplos. Você precisará implementar getters apropriados no seu ActuatorManager
// que leiam o estado atual dos pinos dos relés.
bool getLightStateFromActuator(ActuatorManager* actMgr) {
    if (!actMgr) return false;
    // Exemplo: Supondo que você adicione um método ao ActuatorManager
    // return actMgr->isLightRelayActive();
    // Por enquanto, vamos simular ou retornar um valor fixo se não tiver o método
    // Este é um PONTO IMPORTANTE: você precisa ler o estado REAL do pino do relé no ActuatorManager
    int lightPin = actMgr->gpioConfig.lightControlPin; // Acessando via config (exemplo)
    if (lightPin >= 0) {
        return digitalRead(lightPin) == HIGH; // Ou LOW, dependendo da sua lógica de relé
    }
    return false;
}

bool getHumidifierStateFromActuator(ActuatorManager* actMgr) {
    if (!actMgr) return false;
    // Exemplo:
    // return actMgr->isHumidifierRelayActive();
    int humidifierPin = actMgr->gpioConfig.humidityControlPin; // Acessando via config (exemplo)
    if (humidifierPin >= 0) {
         return digitalRead(humidifierPin) == HIGH; // Ou LOW
    }
    return false;
}
// --- Fim dos métodos auxiliares ---


void WebServerManager::begin() {
    if (!LittleFS.begin()) {
        Serial.println("WebServerManager ERROR: LittleFS mount failed. Web server not started.");
        return;
    }
    Serial.println("WebServerManager: LittleFS mounted.");

    // Servir arquivos estáticos da raiz do LittleFS (onde estarão index.html, style.css, script.js)
    // A pasta `web` (ou `data`) do seu projeto será o conteúdo da raiz "/" no LittleFS.
    server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Rota para API de Sensores
    server_.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!sensorManager_) {
            request->send(500, "application/json", "{\"error\":\"SensorManager not available\"}");
            return;
        }
        DynamicJsonDocument doc(256); // Ajuste o tamanho conforme necessário
        doc["temperature"] = sensorManager_->getTemperature();
        doc["airHumidity"] = sensorManager_->getHumidity();
        doc["soilHumidity"] = sensorManager_->getSoilHumidity();
        doc["vpd"] = sensorManager_->getVpd();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    // Rota para API de Status dos Atuadores e Alvos de Luz/Umidade
    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!targetDataManager_ || !actuatorManager_) {
            request->send(500, "application/json", "{\"error\":\"DataManager or ActuatorManager not available\"}");
            return;
        }
        DynamicJsonDocument doc(512); // Ajuste o tamanho

        JsonObject light = doc.createNestedObject("light");
        light["isOn"] = getLightStateFromActuator(actuatorManager_); // Implementar getter no ActuatorManager
        struct tm lightOn = targetDataManager_->getLightOnTime();
        struct tm lightOff = targetDataManager_->getLightOffTime();
        char timeBuf[6];
        sprintf(timeBuf, "%02d:%02d", lightOn.tm_hour, lightOn.tm_min);
        light["onTime"] = String(timeBuf);
        sprintf(timeBuf, "%02d:%02d", lightOff.tm_hour, lightOff.tm_min);
        light["offTime"] = String(timeBuf);

        JsonObject humidifier = doc.createNestedObject("humidifier");
        humidifier["isOn"] = getHumidifierStateFromActuator(actuatorManager_); // Implementar getter no ActuatorManager
        humidifier["targetAirHumidity"] = targetDataManager_->getTargetAirHumidity();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    // Rota para API de Targets (POST)
    // Usando um JSON body parser
    server_.on("/api/targets", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL, // Sem handler de upload de arquivo
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Este é o handler para o corpo da requisição (quando não é JSON)
            // Para JSON, usamos o onJson abaixo, mas precisamos de um handler de corpo mesmo que vazio.
            // Ou melhor, usar o onJson diretamente:
        }
    );
    // Attach a JSON handler to the /api/targets POST route
    AsyncCallbackJsonWebHandler* jsonHandler = new AsyncCallbackJsonWebHandler("/api/targets", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!targetDataManager_) {
            request->send(500, "application/json", "{\"success\":false, \"message\":\"TargetDataManager not available\"}");
            return;
        }

        const JsonObject& jsonObj = json.as<JsonObject>(); // Ou JsonDocument se for o root
        // Se você estiver usando ArduinoJson v6, o JsonVariant já pode ser o JsonObject
        // Se for v7+, deserializeJson(doc, data) e passar doc para targetDataManager.

        // Para ArduinoJson v6 (que parece ser o caso com PubSubClient e outras libs mais antigas)
        // Se o `json` variant já é o objeto raiz:
        StaticJsonDocument<512> doc; // Crie um documento para passar ao seu manager
                                     // Ou modifique updateTargetsFromJson para aceitar JsonObjectConst
        // Copiando do JsonVariant para um JsonDocument que seu manager espera
        // Isso pode variar dependendo da versão exata do ArduinoJson e como seu TargetDataManager espera
        // Se updateTargetsFromJson espera um JsonDocument:
        DeserializationError error = deserializeJson(doc, (const char*)request->_tempObject); // _tempObject é um hack, melhor parsear 'data'
                                                                                             // na verdade, o JsonVariant 'json' já é o documento parseado.

        // Simplesmente passando o JsonVariant que já é o documento parseado (ou seu root)
        // Se o targetDataManager->updateTargetsFromJson espera JsonDocument:
        // Precisamos converter o JsonVariant para um JsonDocument.
        // Ou, melhor, modificar updateTargetsFromJson para aceitar JsonObjectConst.
        // Por enquanto, vamos assumir que podemos reconstruir um doc para passar.
        // A maneira mais limpa seria o `json` ser um `JsonDocument&`
        // Mas AsyncWebServer passa JsonVariant.

        // Supondo que 'json' é um JsonObject e updateTargetsFromJson pode lidar com isso
        // ou que você pode converter/copiar para um JsonDocument.
        // Exemplo:
        JsonDocument receivedDoc; // Use DynamicJsonDocument para simplicidade
        receivedDoc.set(json); // Copia o conteúdo do JsonVariant para um novo JsonDocument

        bool success = targetDataManager_->updateTargetsFromJson(receivedDoc);

        if (success) {
            request->send(200, "application/json", "{\"success\":true, \"message\":\"Targets updated successfully.\"}");
            // Após atualizar os alvos, podemos querer enviar um evento SSE de status
            this->sendStatusUpdateEvent();
        } else {
            request->send(400, "application/json", "{\"success\":false, \"message\":\"Error updating targets or no valid data.\"}");
        }
    });
    server_.addHandler(jsonHandler);


    // Rota para Server-Sent Events
    events_.onConnect([this](AsyncEventSourceClient *client){
        Serial.println("WebServerManager: SSE Client connected");
        // Enviar estado inicial imediatamente após a conexão
        sendSensorUpdateEvent(); // Envia o estado atual dos sensores
        sendStatusUpdateEvent(); // Envia o estado atual dos atuadores/alvos
    });
    server_.addHandler(&events_);

    // Handler para Not Found (404)
    server_.onNotFound([this](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Resource Not Found");
    });

    // Iniciar o servidor
    server_.begin();
    Serial.println("WebServerManager: HTTP server started.");
}


void WebServerManager::sendSensorUpdateEvent() {
    if (!sensorManager_ || events_.count() == 0) { // Só envia se houver clientes SSE
        return;
    }
    // Controle de frequência (opcional)
    // if (millis() - lastSensorEventTime_ < eventInterval_ && lastSensorEventTime_ != 0) {
    //     return;
    // }

    DynamicJsonDocument doc(256);
    doc["temperature"] = sensorManager_->getTemperature();
    doc["airHumidity"] = sensorManager_->getHumidity();
    doc["soilHumidity"] = sensorManager_->getSoilHumidity();
    doc["vpd"] = sensorManager_->getVpd();

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "sensor_update", millis());
    lastSensorEventTime_ = millis();
    // Serial.println("SSE: Sent sensor_update");
}

void WebServerManager::sendStatusUpdateEvent() {
    if (!targetDataManager_ || !actuatorManager_ || events_.count() == 0) {
        return;
    }
    // if (millis() - lastStatusEventTime_ < eventInterval_ && lastStatusEventTime_ != 0) {
    //     return;
    // }

    DynamicJsonDocument doc(512);
    JsonObject light = doc.createNestedObject("light");
    light["isOn"] = getLightStateFromActuator(actuatorManager_);
    struct tm lightOn = targetDataManager_->getLightOnTime();
    struct tm lightOff = targetDataManager_->getLightOffTime();
    char timeBuf[6];
    sprintf(timeBuf, "%02d:%02d", lightOn.tm_hour, lightOn.tm_min);
    light["onTime"] = String(timeBuf);
    sprintf(timeBuf, "%02d:%02d", lightOff.tm_hour, lightOff.tm_min);
    light["offTime"] = String(timeBuf);

    JsonObject humidifier = doc.createNestedObject("humidifier");
    humidifier["isOn"] = getHumidifierStateFromActuator(actuatorManager_);
    humidifier["targetAirHumidity"] = targetDataManager_->getTargetAirHumidity();

    String jsonString;
    serializeJson(doc, jsonString);
    events_.send(jsonString.c_str(), "status_update", millis());
    lastStatusEventTime_ = millis();
    // Serial.println("SSE: Sent status_update");
}

} // namespace GrowController