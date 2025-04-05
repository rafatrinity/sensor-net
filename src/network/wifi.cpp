// wifi.cpp
#include "wifi.hpp"             // Declaração da função connectToWiFi
#include "config.hpp"           // Para acessar appConfig.wifi (globalmente, por enquanto)
#include "ui/displayManager.hpp" // Para interagir com o display LCD

#include <Arduino.h>           // Para Serial, vTaskDelay, etc.
#include <WiFi.h>              // Para a biblioteca WiFi do ESP32

// #include "utils/spinner.hpp" // REMOVIDO - Lógica do spinner agora no DisplayManager
// #include <LiquidCrystal_I2C.h> // REMOVIDO - Não acessamos mais o LCD diretamente

// extern SemaphoreHandle_t lcdMutex; // REMOVIDO - Mutex agora interno ao DisplayManager
extern AppConfig appConfig;       // AINDA PRESENTE - Necessário para as credenciais

void connectToWiFi(void * parameter) {
    const uint32_t maxRetries = 20;
    const uint32_t retryDelayMs = 500; // Delay entre tentativas em ms

    while (true) { // Loop principal para tentativas de conexão
        Serial.println("Connecting to WiFi...");
        displayManagerShowConnectingWiFi(); // <--- Usa Display Manager para mostrar status inicial

        // Inicia a conexão WiFi usando as credenciais da AppConfig
        WiFi.begin(appConfig.wifi.ssid, appConfig.wifi.password);

        uint32_t attempt = 0;
        // Loop para aguardar a conexão, com limite de tentativas
        while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
            vTaskDelay(pdMS_TO_TICKS(retryDelayMs)); // Pausa não bloqueante
            Serial.print(".");
            // Atualiza o spinner no display para feedback visual
            displayManagerUpdateSpinner(); // <--- Usa Display Manager para o spinner
            attempt++;
        }

        // Verifica se a conexão foi bem-sucedida
        if (WiFi.status() == WL_CONNECTED) {
            String ipAddr = WiFi.localIP().toString(); // Obtém o IP como String
            Serial.println(ipAddr.c_str()); // Imprime o IP no Serial

            // Mostra o IP no display usando o DisplayManager
            displayManagerShowWiFiConnected(ipAddr.c_str()); // <--- Usa Display Manager

            // Pequena pausa opcional para visualizar o IP no display
            // vTaskDelay(pdMS_TO_TICKS(2000));
            // displayManagerClear(); // Opcional: Limpar após mostrar o IP

            break; // Sai do loop while(true) pois conectou com sucesso
        } else {
            // Falha ao conectar após maxRetries
            Serial.println("\nFailed to connect to WiFi, retrying in 5 seconds...");
            displayManagerShowError("WiFi Fail"); // <--- Mostra erro no display
            vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarda 5 segundos antes de tentar novamente
            displayManagerClear(); // Limpa a mensagem de erro antes da próxima tentativa
        }
    }

    // Conexão bem-sucedida, a tarefa não é mais necessária
    Serial.println("WiFi Task finished.");
    vTaskDelete(NULL); // Deleta a própria tarefa
}