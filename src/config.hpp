#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <LiquidCrystal_I2C.h>
#include <DHT.h>
// Inclua FreeRTOS se estiver usando Semaphores diretamente aqui
// Ou melhor, passe os semáforos como dependências em vez de extern
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// --- Inclusão Condicional das Configurações da Placa ---
#if defined(ARDUINO_ESP32_DEV)
    #include "boards/board_esp32_dev.hpp"
    // Define para identificar a placa no código, se necessário
    #define BOARD_NAME "ESP32 DevKit V1"
#elif defined(ARDUINO_SEEED_XIAO_ESP32C3) // Use o define específico da placa XIAO C3
    #include "boards/board_xiao_c3.hpp"
    // Define para identificar a placa no código, se necessário
    #define BOARD_NAME "Seeed XIAO ESP32-C3"
#else
    #error "Placa não suportada. Crie um arquivo 'src/boards/board_your_board_name.hpp' e inclua-o condicionalmente em config.hpp."
#endif
// ---------------------------------------------------------

extern LiquidCrystal_I2C LCD; // TODO: Considerar encapsular em uma classe DisplayManager

// Credenciais do WiFi (Mantém a lógica de UNIT_TEST)
#ifdef UNIT_TEST
    #define WIFI_SSID "Wokwi-GUEST"
    #define WIFI_PASSWORD ""
#else
    #define WIFI_SSID "Casa" // Ou use uma variável de ambiente/arquivo de segredos
    #define WIFI_PASSWORD "12345678" // Definitivamente use uma forma mais segura!
#endif

// --- Configurações Gerais da Aplicação ---

#define BAUD 115200 // Taxa de comunicação serial

// Semáforos (Ainda como extern - considere injeção de dependência)
extern SemaphoreHandle_t mqttMutex;
extern SemaphoreHandle_t lcdMutex;

// Estruturas de Configuração (Usam os #defines das placas incluídas acima)
struct WiFiConfig {
    const char* ssid = WIFI_SSID;
    const char* password = WIFI_PASSWORD;
};

struct MQTTConfig {
    const char* server = "192.168.1.11"; // Pode ser configurável em tempo de execução? (NVS)
    int port = 1883;
    const char* clientId = MQTT_CLIENT_ID; // Vem do board_xxx.hpp
    const char* roomTopic = MQTT_ROOM_TOPIC; // Vem do board_xxx.hpp
    // Adicionar tópicos específicos aqui, se necessário
    // Ex: const char* tempTopic = MQTT_ROOM_TOPIC "/temperature";
};

struct GPIOControlConfig {
    int humidityControlPin = GPIO_HUMIDITY_PIN; // Vem do board_xxx.hpp
    int lightControlPin = GPIO_LIGHT_PIN;     // Vem do board_xxx.hpp
};

struct TimeConfig {
    long utcOffsetInSeconds = -10800; // Fuso horário (-3 GMT)
    const char* ntpServer = "pool.ntp.org"; // Servidor NTP
};

struct SensorConfig {
    int dhtPin = DHT_PIN;                 // Vem do board_xxx.hpp
    int dhtType = DHT22;                  // Tipo do sensor DHT
    int soilHumiditySensorPin = SOIL_HUMIDITY_PIN; // Vem do board_xxx.hpp
    // Adicionar aqui configurações de calibração do sensor de solo, se houver
    // Ex: int soilMinReading = 1000;
    // Ex: int soilMaxReading = 3000;
};

// Estrutura de Configuração Principal da Aplicação
struct AppConfig {
    WiFiConfig wifi;
    MQTTConfig mqtt;
    GPIOControlConfig gpioControl;
    TimeConfig time;
    SensorConfig sensor;
    // Poderia incluir o nome da placa aqui
    // const char* board = BOARD_NAME;
};

// Instância global da configuração (Ainda como extern - considere injeção)
extern AppConfig appConfig;

#endif // CONFIG_HPP