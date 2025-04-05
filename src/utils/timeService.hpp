#ifndef TIME_SERVICE_HPP
#define TIME_SERVICE_HPP

#include "config.hpp" // Para a struct TimeConfig
#include <time.h>     // Para a struct tm

/**
 * @brief Inicializa o serviço de tempo, configurando o NTP.
 * Deve ser chamado após a conexão WiFi ser estabelecida.
 *
 * @param config Referência à estrutura TimeConfig contendo o offset UTC e o servidor NTP.
 * @return true Se a configuração inicial do NTP foi chamada com sucesso.
 * @return false Se houve um problema (ex: config inválida - embora raro aqui).
 */
bool initializeTimeService(const TimeConfig& config);

/**
 * @brief Obtém a hora local atual sincronizada via NTP.
 * Tenta obter a hora atual do sistema, que deve estar sendo sincronizada
 * pelo cliente SNTP do ESP32 (configurado em initializeTimeService).
 *
 * @param timeinfo Referência a uma struct tm que será preenchida com a hora atual se a função for bem-sucedida.
 * @return true Se a hora local foi obtida com sucesso (implica que o NTP está sincronizado).
 * @return false Se não foi possível obter a hora local (NTP pode não estar sincronizado ainda).
 */
bool getCurrentTime(struct tm timeinfo);

/**
 * @brief Verifica se o serviço de tempo foi inicializado com sucesso.
 * Útil para saber se initializeTimeService foi chamado e completado.
 * Não garante que o NTP esteja *atualmente* sincronizado, apenas que a tentativa inicial ocorreu.
 *
 * @return true Se o serviço foi inicializado.
 * @return false Se o serviço ainda não foi inicializado.
 */
bool isTimeServiceInitialized();


#endif // TIME_SERVICE_HPP