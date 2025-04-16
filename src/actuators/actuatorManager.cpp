// src/actuators/actuatorManager.cpp
#include "actuatorManager.hpp"
#include "utils/timeService.hpp" // Para getCurrentTime
#include <Arduino.h>           // Para pinMode, digitalWrite, Serial, etc.
#include <math.h>              // Para isnan
#include <time.h>

namespace GrowController {

// Definição das constantes estáticas
const TickType_t ActuatorManager::LIGHT_CHECK_INTERVAL_MS = pdMS_TO_TICKS(5000);     // 5 segundos
const TickType_t ActuatorManager::HUMIDITY_CHECK_INTERVAL_MS = pdMS_TO_TICKS(10000); // 10 segundos

// --- Construtor e Destrutor ---

ActuatorManager::ActuatorManager(const GPIOControlConfig& config,
                                 TargetDataManager& targetMgr,
                                 SensorManager& sensorMgr) :
    gpioConfig(config),
    targetDataManager(targetMgr),
    sensorManager(sensorMgr),
    lastLightState(-1), // Ou LOW / HIGH se preferir
    lightTaskHandle(nullptr),
    humidityTaskHandle(nullptr),
    initialized(false)
{}

ActuatorManager::~ActuatorManager() {
    // Parar tarefas se estiverem rodando
    if (lightTaskHandle != nullptr) {
        vTaskDelete(lightTaskHandle);
        lightTaskHandle = nullptr;
    }
    if (humidityTaskHandle != nullptr) {
        vTaskDelete(humidityTaskHandle);
        humidityTaskHandle = nullptr;
    }
    Serial.println("ActuatorManager: Destroyed.");
}

// --- Inicialização ---

bool ActuatorManager::initialize() {
     if (initialized) {
        Serial.println("ActuatorManager WARN: Already initialized.");
        return true;
    }
    Serial.println("ActuatorManager: Initializing Actuator GPIOs...");

    // Configura Pino da Luz
    Serial.printf(" - Light Control Pin: %d\n", gpioConfig.lightControlPin);
    pinMode(gpioConfig.lightControlPin, OUTPUT);
    digitalWrite(gpioConfig.lightControlPin, LOW); // Estado inicial desligado

    // Configura Pino de Umidade
    Serial.printf(" - Humidity Control Pin: %d\n", gpioConfig.humidityControlPin);
    pinMode(gpioConfig.humidityControlPin, OUTPUT);
    digitalWrite(gpioConfig.humidityControlPin, LOW); // Estado inicial desligado

    initialized = true;
    Serial.println("ActuatorManager: GPIOs Initialized.");
    return true; // Assumindo sucesso
}

bool ActuatorManager::isInitialized() const {
     return initialized;
}


// --- Métodos de Controle (Internos) ---

void ActuatorManager::checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int lightPin) {
    struct tm timeinfo = {0}; // Inicializar struct
    // Usa o getCurrentTime global (precisa ser refatorado para TimeService injetado eventualmente)
    if (!getCurrentTime(timeinfo)) {
        Serial.println("ActuatorManager WARN: Failed to get current time for light control!");
        // Considerar uma ação segura? Manter estado atual? Desligar?
        // digitalWrite(lightPin, LOW); // Ex: Desligar como segurança
        return;
    }

    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int startMinutes = lightOn.tm_hour * 60 + lightOn.tm_min;
    int endMinutes = lightOff.tm_hour * 60 + lightOff.tm_min;
    bool shouldBeOn = false;

    if (startMinutes == endMinutes) {
        shouldBeOn = false; // Sempre desligado se on=off
    } else if (startMinutes < endMinutes) { // Horário diurno
        shouldBeOn = (nowMinutes >= startMinutes && nowMinutes < endMinutes);
    } else { // Horário noturno (atravessa meia-noite)
        shouldBeOn = (nowMinutes >= startMinutes || nowMinutes < endMinutes);
    }

    // Aplica o estado e loga apenas na mudança
    int desiredState = shouldBeOn ? HIGH : LOW;
    // Ler estado atual para evitar escritas desnecessárias? Opcional.
    // int currentState = digitalRead(lightPin);
    // if (desiredState != currentState) { ... }

    digitalWrite(lightPin, desiredState);

    if (desiredState != this->lastLightState) {
        Serial.printf("ActuatorManager: Light state changed to %s (Pin: %d, Schedule: %02d:%02d-%02d:%02d, Now: %02d:%02d)\n",
                      shouldBeOn ? "ON" : "OFF",
                      lightPin,
                      lightOn.tm_hour, lightOn.tm_min,
                      lightOff.tm_hour, lightOff.tm_min,
                      timeinfo.tm_hour, timeinfo.tm_min);
        this->lastLightState = desiredState;
    }
}

void ActuatorManager::checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin) {
    // Validação (igual à anterior)
    if (isnan(currentHumidity) || isnan(targetHumidity) || targetHumidity <= 0.0f) {
        if (digitalRead(humidityPin) == HIGH) {
            digitalWrite(humidityPin, LOW);
            Serial.printf("ActuatorManager: Humidifier turned OFF due to invalid data (Current: %.1f, Target: %.1f, Pin: %d)\n",
                          currentHumidity, targetHumidity, humidityPin);
        }
        return;
    }

    // Lógica de controle (igual à anterior)
    bool shouldBeOn = (currentHumidity < targetHumidity);
    int currentState = digitalRead(humidityPin);
    int desiredState = shouldBeOn ? HIGH : LOW;

    if (desiredState != currentState) {
        digitalWrite(humidityPin, desiredState);
        Serial.printf("ActuatorManager: Humidifier state changed to %s (Current: %.1f%%, Target: %.1f%%, Pin: %d)\n",
                      shouldBeOn ? "ON" : "OFF",
                      currentHumidity, targetHumidity, humidityPin);
    }
}

// --- Gerenciamento das Tarefas ---

bool ActuatorManager::startControlTasks(UBaseType_t lightTaskPriority,
                                        UBaseType_t humidityTaskPriority,
                                        uint32_t stackSize)
{
     if (!initialized) {
         Serial.println("ActuatorManager ERROR: Cannot start tasks, manager not initialized.");
         return false;
    }
    if (lightTaskHandle != nullptr || humidityTaskHandle != nullptr) {
        Serial.println("ActuatorManager WARN: Tasks already started.");
        return true; // Ou false?
    }

    Serial.println("ActuatorManager: Starting control tasks...");
    bool success = true;

    // Inicia Tarefa de Luz
    BaseType_t lightResult = xTaskCreate(
        lightControlTaskWrapper,
        "LightCtrlTask",
        stackSize, // Usar stack size comum ou individual?
        this,      // Passa ponteiro da instância
        lightTaskPriority,
        &this->lightTaskHandle
    );
    if (lightResult != pdPASS) {
        lightTaskHandle = nullptr;
        Serial.print("ActuatorManager ERROR: Failed to create Light Control task! Code: ");
        Serial.println(lightResult);
        success = false;
    }

    // Inicia Tarefa de Umidade
    BaseType_t humidityResult = xTaskCreate(
        humidityControlTaskWrapper,
        "HumidCtrlTask",
        stackSize,
        this,
        humidityTaskPriority,
        &this->humidityTaskHandle
    );
     if (humidityResult != pdPASS) {
        humidityTaskHandle = nullptr;
        Serial.print("ActuatorManager ERROR: Failed to create Humidity Control task! Code: ");
        Serial.println(humidityResult);
        success = false;
    }

    if (success) {
        Serial.println("ActuatorManager: Control tasks started.");
    } else {
         // Se uma falhou, talvez parar a outra?
         if (lightTaskHandle) { vTaskDelete(lightTaskHandle); lightTaskHandle = nullptr; }
         // (humidityTaskHandle já seria null se falhou)
    }

    return success;
}


// --- Implementação das Tarefas (Wrappers e Loops) ---

void ActuatorManager::lightControlTaskWrapper(void* pvParameters) {
     ActuatorManager* instance = static_cast<ActuatorManager*>(pvParameters);
     if (instance) {
         instance->runLightControlTask();
     }
     vTaskDelete(NULL); // Garante término
}

void ActuatorManager::runLightControlTask() {
    Serial.println("ActuatorManager: Light Control Task started.");
    while (true) {
        // Obtém alvos do TargetDataManager injetado
        struct tm onTime = targetDataManager.getLightOnTime();
        struct tm offTime = targetDataManager.getLightOffTime();

        // Chama método de controle interno
        checkAndControlLight(onTime, offTime, gpioConfig.lightControlPin);

        vTaskDelay(LIGHT_CHECK_INTERVAL_MS);
    }
}


void ActuatorManager::humidityControlTaskWrapper(void* pvParameters) {
    ActuatorManager* instance = static_cast<ActuatorManager*>(pvParameters);
     if (instance) {
         instance->runHumidityControlTask();
     }
     vTaskDelete(NULL);
}

void ActuatorManager::runHumidityControlTask() {
    Serial.println("ActuatorManager: Humidity Control Task started.");
    while (true) {
        // Obtém umidade atual do SensorManager injetado (THREAD-SAFE getter)
        float currentAirHumidity = sensorManager.getHumidity();

        // Obtém umidade alvo do TargetDataManager injetado (THREAD-SAFE getter)
        float targetAirHumidity = targetDataManager.getTargetAirHumidity();

        // Chama método de controle interno
        checkAndControlHumidity(currentAirHumidity, targetAirHumidity, gpioConfig.humidityControlPin);

        vTaskDelay(HUMIDITY_CHECK_INTERVAL_MS);
    }
}

} // namespace GrowController