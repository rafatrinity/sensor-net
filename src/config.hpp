#ifndef CONFIG_HPP
#define CONFIG_HPP

// Includes de bibliotecas ainda podem ser necessários por outros módulos
// que usam tipos definidos aqui, mas não necessariamente o LCD diretamente.
#include <DHT.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <IPAddress.h>

// --- Inclusão Condicional das Configurações da Placa ---
#if defined(ARDUINO_ESP32_DEV)
    #include "boards/board_esp32_dev.hpp"
    #define BOARD_NAME "ESP32 DevKit V1"
#elif defined(ARDUINO_SEEED_XIAO_ESP32C3)
    #include "boards/board_xiao_c3.hpp"
     #define BOARD_NAME "Seeed XIAO ESP32-C3"
#else
    #error "Placa não suportada. Crie um arquivo 'src/boards/board_your_board_name.hpp' e inclua-o condicionalmente em config.hpp."
#endif

// Credenciais do WiFi (Mantém a lógica de UNIT_TEST)
#ifdef UNIT_TEST
    #define WIFI_SSID "Wokwi-GUEST"
    #define WIFI_PASSWORD ""
#else
    #define WIFI_SSID "Casa" // TODO: Mudar para método seguro
    #define WIFI_PASSWORD "12345678" // TODO: Mudar para método seguro
#endif

#define STATIC_IP_ENABLED 1 // 1 para habilitar IP estático, 0 para DHCP (se loadWiFiCredentials falhar)

#if STATIC_IP_ENABLED
    const IPAddress LOCAL_IP(192, 168, 1, 180);
    const IPAddress GATEWAY_IP(192, 168, 1, 1);
    const IPAddress SUBNET_MASK(255, 255, 255, 0);
    const IPAddress PRIMARY_DNS_IP(8, 8, 8, 8);
    const IPAddress SECONDARY_DNS_IP(8, 8, 4, 4);
#endif

// --- Configurações do Servidor Web e mDNS ---
#define HTTP_PORT 80
#define MDNS_HOSTNAME "greenhouse"
#define MDNS_SERVICE_NAME "webserver"

    // --- Configurações Gerais da Aplicação ---

#define BAUD 115200

    // --- Initialization Retry Configuration ---
#define INIT_RETRY_COUNT 3       // Number of initialization attempts
#define INIT_RETRY_DELAY_MS 1000 // Delay between attempts in milliseconds

    // Semáforos (Ainda como extern - próximos alvos de refatoração)
    extern SemaphoreHandle_t mqttMutex; // OK por enquanto, será encapsulado no MqttManager

    // Estruturas de Configuração
    struct WiFiConfig
    {
        const char *ssid = WIFI_SSID;
        const char *password = WIFI_PASSWORD;
    };

struct MQTTConfig {
    const char* server = MQTT_SERVER; 
    int port = MQTT_PORT;             
    const char* clientId = MQTT_CLIENT_ID;
    const char* roomTopic = MQTT_ROOM_TOPIC;
};

struct GPIOControlConfig {
    int humidityControlPin = GPIO_HUMIDITY_PIN;
    int lightControlPin = GPIO_LIGHT_PIN;
};

struct TimeConfig {
    long utcOffsetInSeconds = -10800;
    const char* ntpServer = "pool.ntp.org";
};

struct SensorConfig {
    int dhtPin = DHT_PIN;
    int dhtType = DHT22;
    int soilHumiditySensorPin = SOIL_HUMIDITY_PIN;
};

// Estrutura de Configuração Principal da Aplicação
struct AppConfig {
    WiFiConfig wifi;
    MQTTConfig mqtt;
    GPIOControlConfig gpioControl;
    TimeConfig time;
    SensorConfig sensor;
    // const char* board = BOARD_NAME; // Pode adicionar se precisar
};
#endif // CONFIG_HPP