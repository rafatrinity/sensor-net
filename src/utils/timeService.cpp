#include "timeService.hpp"
#include <Arduino.h> // Para Serial e funções de tempo do ESP-IDF via Arduino core
#include <time.h>    // Para configTime, getLocalTime, struct tm
#include <string.h>  // Para strlen

namespace GrowController {

// --- Construtor ---
TimeService::TimeService() : serviceInitializedState(false) {
    // Inicialização básica, o trabalho pesado fica no método initialize()
}

// --- Destrutor ---
TimeService::~TimeService() {
    // Nada específico para limpar aqui no momento
}

// --- Inicialização ---
/**
 * @brief Inicializa o serviço de tempo, configurando o NTP.
 */
bool TimeService::initialize(const TimeConfig& config) {
    if (serviceInitializedState) {
        Serial.println("TimeService WARN: Already initialized.");
        // Poderia retornar true ou tentar re-inicializar, dependendo da necessidade.
        // Retornar true indica que já está pronto para uso.
        return true;
    }

    if (config.ntpServer == nullptr || strlen(config.ntpServer) == 0) {
        Serial.println("TimeService ERROR: Invalid NTP server in configuration.");
        return false; // Falha na configuração
    }

    Serial.print("TimeService: Configuring NTP with server: ");
    Serial.print(config.ntpServer);
    Serial.print(", UTC Offset: ");
    Serial.println(config.utcOffsetInSeconds);

    // Configura o cliente SNTP interno do ESP-IDF
    configTime(config.utcOffsetInSeconds, 0, config.ntpServer);

    // Tenta obter a hora inicial para verificar se a configuração funcionou
    Serial.println("TimeService: Waiting for initial NTP sync...");
    struct tm timeinfo;
    int retryCount = 0;
    const int maxRetries = 10; // Tenta por até ~10 segundos
    // Aumentar um pouco o timeout de getLocalTime pode ajudar na primeira sincronização
    while (!getLocalTime(&timeinfo, 1000) && retryCount < maxRetries) { // Espera 1000ms por tentativa
         Serial.print(".");
         retryCount++;
         // O delay explícito aqui pode não ser mais necessário com o timeout maior em getLocalTime
         // delay(500);
    }

    if (retryCount >= maxRetries) {
        Serial.println("\nTimeService ERROR: Failed to obtain NTP time after multiple retries.");
        // Marcamos como inicializado mesmo em falha de sincronização,
        // pois a configuração foi feita e getCurrentTime pode tentar novamente.
        this->serviceInitializedState = true; // Configuração realizada
        return true; // Retorna true indicando que a *configuração* foi feita.
                     // A aplicação pode verificar a sincronização real com getCurrentTime.
    } else {
        Serial.println("\nTimeService: Initial NTP time synchronized successfully!");
        Serial.print("TimeService: Current Time: ");
        // Assegura que timeinfo não é null antes de usar com asprintf (embora não deva ser)
        char timeBuffer[64];
        strftime(timeBuffer, sizeof(timeBuffer), "%A, %B %d %Y %H:%M:%S", &timeinfo);
        Serial.println(timeBuffer);

        this->serviceInitializedState = true; // Serviço configurado e sincronizado inicialmente
        return true;
    }
}

// --- Obtenção da Hora ---
/**
 * @brief Obtém a hora local atual sincronizada via NTP.
 */
bool TimeService::getCurrentTime(struct tm& timeinfo) const {
    if (!this->serviceInitializedState) {
        // Evitar log excessivo aqui, pois pode ser chamado frequentemente antes da inicialização
        // Serial.println("TimeService WARN: Attempted to get time before initialization.");
        return false; // Não tenta obter a hora se o serviço nem foi configurado
    }

    // Tenta obter a hora local. Usar um timeout pequeno (e.g., 10ms) aqui é geralmente bom
    // para não bloquear a thread chamadora se o NTP estiver temporariamente fora de sincronia.
    if (!getLocalTime(&timeinfo, 10)) { // Espera no máximo 10ms
        // Serial.println("TimeService DEBUG: Failed to get current time (NTP potentially out of sync).");
        return false; // Falha ao obter a hora (provavelmente não sincronizado)
    }
    return true; // Hora obtida com sucesso
}

// --- Verificação de Inicialização ---
/**
 * @brief Verifica se o serviço de tempo foi inicializado com sucesso.
 */
bool TimeService::isInitialized() const {
    return this->serviceInitializedState;
}

} // namespace GrowController