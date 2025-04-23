#ifndef TIME_SERVICE_HPP
#define TIME_SERVICE_HPP

#include "config.hpp" // Para a struct TimeConfig
#include <time.h>     // Para a struct tm

namespace GrowController {

/**
 * @brief Classe para gerenciar o serviço de tempo e sincronização NTP.
 * Encapsula a inicialização e obtenção da hora local.
 */
class TimeService {
public:
    /**
     * @brief Construtor padrão.
     */
    TimeService();

    /**
     * @brief Destrutor padrão.
     */
    ~TimeService();

    // Desabilitar cópia e atribuição para evitar problemas de estado
    TimeService(const TimeService&) = delete;
    TimeService& operator=(const TimeService&) = delete;

    /**
     * @brief Inicializa o serviço de tempo, configurando o NTP.
     * Deve ser chamado após a conexão WiFi ser estabelecida.
     *
     * @param config Referência à estrutura TimeConfig contendo o offset UTC e o servidor NTP.
     * @return true Se a configuração inicial do NTP foi chamada e (opcionalmente) a sincronização inicial foi bem-sucedida.
     * @return false Se houve um problema na configuração (ex: config inválida).
     */
    bool initialize(const TimeConfig& config);

    /**
     * @brief Obtém a hora local atual sincronizada via NTP.
     * Tenta obter a hora atual do sistema, que deve estar sendo sincronizada
     * pelo cliente SNTP do ESP32 (configurado em initialize()).
     *
     * @param timeinfo Referência a uma struct tm que será preenchida com a hora atual se a função for bem-sucedida.
     * @return true Se a hora local foi obtida com sucesso (implica que o NTP está sincronizado).
     * @return false Se não foi possível obter a hora local (serviço não inicializado ou NTP não sincronizado ainda).
     */
    bool getCurrentTime(struct tm& timeinfo) const; // Marcado como const

    /**
     * @brief Verifica se o serviço de tempo foi inicializado (tentativa de configuração NTP ocorreu).
     * Não garante que o NTP esteja *atualmente* sincronizado.
     *
     * @return true Se o método initialize() foi chamado com sucesso.
     * @return false Se initialize() ainda não foi chamado ou falhou na configuração.
     */
    bool isInitialized() const; // Marcado como const

private:
    bool serviceInitializedState = false; // Estado interno da classe
    // Poderia armazenar a config aqui se necessário para re-inicialização, mas não é o caso agora.
    // const TimeConfig* timeConfigPtr = nullptr;
};

} // namespace GrowController

#endif // TIME_SERVICE_HPP