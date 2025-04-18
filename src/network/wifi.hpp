// src/network/wifi.hpp
#ifndef WIFI_H
#define WIFI_H

#include "config.hpp"
// Forward declaration para evitar include circular ou pesado, se possível
namespace GrowController {
    class DisplayManager;
}
// #include "ui/displayManager.hpp" // Alternativamente, incluir se forward declaration não bastar

// --- Estrutura de Parâmetros para a Tarefa WiFi ---
struct WiFiTaskParams {
    const WiFiConfig* wifiConfig;                // Ponteiro para a configuração WiFi
    GrowController::DisplayManager* displayMgr; // Ponteiro para o Display Manager
};


/**
 * @brief Função da tarefa FreeRTOS para conectar ao WiFi.
 *        Mostra o status no DisplayManager.
 * @param parameter Ponteiro para uma estrutura WiFiTaskParams.
 */
void connectToWiFi(void* parameter);

#endif // WIFI_H