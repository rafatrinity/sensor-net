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
// As funções abaixo ainda dependem das variáveis globais 'appConfig' e 'target'.
// O ideal é refatorar para usar Injeção de Dependência, passando essas
// informações para as funções ou para uma classe ActuatorManager.
// ==========================================================================
extern AppConfig appConfig;
extern TargetValues target;

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
        // Está ligado se a hora atual está entre o início e o fim (exclusive o fim)
        shouldBeOn = (nowMinutes >= startMinutes && nowMinutes < endMinutes);
    } else { // startMinutes > endMinutes
        // Caso noturno (ex: ligar 20:00, desligar 06:00)
        // Está ligado se a hora atual é maior/igual ao início OU menor que o fim
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
    // Define a frequência de verificação (ex: a cada 5 segundos)
    const TickType_t checkInterval = pdMS_TO_TICKS(5000);
    Serial.println("Light Control Task started.");

    while (true) {
        // Chama a função de lógica de controle, buscando os dados das globais (temporário)
        checkAndControlLight(target.lightOnTime, target.lightOffTime, appConfig.gpioControl.lightControlPin);

        // Aguarda o próximo ciclo de verificação
        vTaskDelay(checkInterval);
    }
}