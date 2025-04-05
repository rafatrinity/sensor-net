#include "actuatorManager.hpp"
#include "config.hpp"           // Para acessar appConfig (global, por enquanto)
#include "sensors/targets.hpp"  // Para acessar target (global, por enquanto)
#include "utils/timeService.hpp"

#include <Arduino.h>
#include <time.h>               // Para getLocalTime()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ==========================================================================
// !!! ATENÇÃO: Dependência de Globais !!!
// A tarefa ainda depende da global 'target'. Isso precisa ser refatorado.
// A dependência de 'appConfig' foi removida da tarefa.
// ==========================================================================
extern TargetValues target; // TODO <<< AINDA GLOBAL - Refatorar!

// Implementação da inicialização
void initializeActuators(const GPIOControlConfig& config) {
    Serial.println("Initializing Actuators...");
    pinMode(config.lightControlPin, OUTPUT);
    digitalWrite(config.lightControlPin, LOW); // Garante que comece desligado

    // Quando mover a lógica de umidade, inicialize o pino aqui também:
    // pinMode(config.humidityControlPin, OUTPUT);
    // digitalWrite(config.humidityControlPin, LOW);

    Serial.println("Actuators Initialized.");
}

// Implementação da lógica de controle da luz
void checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int gpioPin) {
    struct tm timeinfo;
    if (!getCurrentTime(timeinfo)) {
        Serial.println("ActuatorManager: Falha ao obter hora atual para controle de luz!");
        return;
    }

    // Converte a hora atual e os alvos para minutos desde a meia-noite para facilitar a comparação
    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int startMinutes = lightOn.tm_hour * 60 + lightOn.tm_min;
    int endMinutes = lightOff.tm_hour * 60 + lightOff.tm_min;

    bool shouldBeOn = false;

    if (startMinutes == endMinutes) {
        // Se os horários são iguais, a luz nunca liga (ou sempre liga?).
        // Vamos assumir que nunca liga por segurança.
        shouldBeOn = false;
    } else if (startMinutes < endMinutes) {
        // Caso normal (ex: ligar 08:00, desligar 18:00)
        shouldBeOn = (nowMinutes >= startMinutes && nowMinutes < endMinutes);
    } else { // startMinutes > endMinutes
        // Caso noturno (ex: ligar 20:00, desligar 06:00)
        shouldBeOn = (nowMinutes >= startMinutes || nowMinutes < endMinutes);
    }

    // Aplica o estado ao pino GPIO
    digitalWrite(gpioPin, shouldBeOn ? HIGH : LOW);

    // Log opcional para mudança de estado (ajuda na depuração)
    static int lastState = -1; // Usa -1 para forçar log na primeira execução
    int currentState = shouldBeOn ? HIGH : LOW;
    if (currentState != lastState) {
        Serial.printf("ActuatorManager: Luz alterada para %s (%d)\n", shouldBeOn ? "ON" : "OFF", currentState);
        lastState = currentState;
    }
}

// Implementação da tarefa FreeRTOS
void lightControlTask(void *parameter) {
  const GPIOControlConfig* gpioConfig = static_cast<const GPIOControlConfig*>(parameter); // Obtém config do parâmetro
  if (gpioConfig == nullptr) { 
      Serial.println("ActuatorManager: Configuração de GPIO inválida!");
      vTaskDelete(NULL);
   }

  const TickType_t checkInterval = pdMS_TO_TICKS(5000);
  Serial.println("Light Control Task started.");
  
  while (true) {
      checkAndControlLight(target.lightOnTime, target.lightOffTime, gpioConfig->lightControlPin);
      vTaskDelay(checkInterval);
  }
}
