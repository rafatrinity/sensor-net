#include "timeService.hpp"
#include <Arduino.h> // Para Serial e funções de tempo do ESP-IDF via Arduino core
#include <time.h>    // Para configTime, getLocalTime, struct tm

// --- Estado Interno do Módulo ---
static bool serviceInitialized = false;
// ---------------------------------

/**
 * @brief Inicializa o serviço de tempo, configurando o NTP.
 * Deve ser chamado após a conexão WiFi ser estabelecida.
 *
 * @param config Referência à estrutura TimeConfig contendo o offset UTC e o servidor NTP.
 * @return true Se a configuração inicial do NTP foi chamada com sucesso.
 * @return false Se houve um problema.
 */
bool initializeTimeService(const TimeConfig& config) {
    if (config.ntpServer == nullptr || strlen(config.ntpServer) == 0) {
        Serial.println("TimeService ERROR: Servidor NTP inválido na configuração.");
        return false;
    }

    Serial.print("TimeService: Configuring NTP with server: ");
    Serial.print(config.ntpServer);
    Serial.print(", UTC Offset: ");
    Serial.println(config.utcOffsetInSeconds);

    // Configura o cliente SNTP interno do ESP-IDF
    // Parâmetros: gmtOffset_sec, daylightOffset_sec, server1, [server2], [server3]
    configTime(config.utcOffsetInSeconds, 0, config.ntpServer);

    // Tenta obter a hora inicial para verificar se a configuração funcionou
    // e para forçar uma sincronização inicial (pode levar alguns segundos)
    Serial.println("TimeService: Waiting for initial NTP sync...");
    struct tm timeinfo;
    int retryCount = 0;
    const int maxRetries = 10; // Tenta por até ~10 segundos
    while (!getLocalTime(&timeinfo, 500) && retryCount < maxRetries) { // Espera 500ms por tentativa
         Serial.print(".");
         retryCount++;
         // Adiciona um pequeno delay para não sobrecarregar
         // (getLocalTime já tem um timeout implícito, mas vamos garantir)
         delay(500);
    }

    if (retryCount >= maxRetries) {
        Serial.println("\nTimeService ERROR: Failed to obtain NTP time after multiple retries.");
        // Mesmo falhando na sincronização inicial, marcamos como inicializado
        // para permitir novas tentativas via getCurrentTime().
        serviceInitialized = true; // Serviço configurado, mas não sincronizado.
        return true; // Retorna true porque a configuração foi feita.
    } else {
        Serial.println("\nTimeService: Initial NTP time synchronized successfully!");
        Serial.print("TimeService: Current Time: ");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S"); // Formato de exemplo
        serviceInitialized = true;
        return true;
    }
}

/**
 * @brief Obtém a hora local atual sincronizada via NTP.
 *
 * @param timeinfo Referência a uma struct tm que será preenchida com a hora atual.
 * @return true Se a hora local foi obtida com sucesso.
 * @return false Se não foi possível obter a hora local.
 */
bool getCurrentTime(struct tm& timeinfo) {
    if (!serviceInitialized) {
        Serial.println("TimeService WARN: Attempted to get time before initialization.");
        return false; // Não tenta obter a hora se o serviço nem foi configurado
    }

    // Tenta obter a hora local. O timeout é opcional (0 = não espera).
    // Usar 0 é mais rápido se você só quer checar se já sincronizou.
    // Usar um valor pequeno (ex: 10) pode ajudar se a sincronia estiver marginal.
    if (!getLocalTime(timeinfo, 10)) { // Espera no máximo 10ms
        // Serial.println("TimeService: Failed to get current time (NTP potentially out of sync).");
        return false; // Falha ao obter a hora (provavelmente não sincronizado)
    }
    return true; // Hora obtida com sucesso
}

/**
 * @brief Verifica se o serviço de tempo foi inicializado com sucesso.
 *
 * @return true Se o serviço foi inicializado.
 * @return false Se o serviço ainda não foi inicializado.
 */
bool isTimeServiceInitialized() {
    return serviceInitialized;
}