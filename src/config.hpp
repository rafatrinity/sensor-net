#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <LiquidCrystal_I2C.h>
#include <DHT.h>

extern LiquidCrystal_I2C LCD;

// Credenciais do WiFi
#ifdef UNIT_TEST
    #define WIFI_SSID "Wokwi-GUEST"
    #define WIFI_PASSWORD ""
#else
    #define WIFI_SSID "Casa"
    #define WIFI_PASSWORD "12345678"
#endif

#if defined(ARDUINO_ESP32_DEV)
    #define GPIO_HUMIDITY_PIN 2
    #define GPIO_LIGHT_PIN 5
    #define DHT_PIN 4
    #define SOIL_HUMIDITY_PIN 34
    #define SDA 33
    #define SCL 32
    #define MQTT_ROOM_TOPIC "01"
    #define MQTT_CLIENT_ID "ESP32Client1"
#else
    #define GPIO_HUMIDITY_PIN 0
    #define GPIO_LIGHT_PIN 4
    #define DHT_PIN 10
    #define SOIL_HUMIDITY_PIN 2
    #define SDA 8
    #define SCL 9
    #define MQTT_ROOM_TOPIC "02"
    #define MQTT_CLIENT_ID "ESP32Client2"
// #else
//     #error "Placa não suportada. Defina as configurações para sua placa."
#endif

#define BAUD 115200

extern SemaphoreHandle_t mqttMutex;
extern SemaphoreHandle_t lcdMutex;
struct WiFiConfig {
    // Usa os macros definidos para credenciais
    const char* ssid = WIFI_SSID;
    const char* password = WIFI_PASSWORD;
};

struct MQTTConfig {
    const char* server = "192.168.1.11";
    int port = 1883;
    const char* clientId = MQTT_CLIENT_ID;
    const char* roomTopic = MQTT_ROOM_TOPIC;
};

struct GPIOControlConfig {
    int humidityControlPin = GPIO_HUMIDITY_PIN;
    int lightControlPin = GPIO_LIGHT_PIN;
};

struct TimeConfig {
    long utcOffsetInSeconds = -10800;
};

struct SensorConfig {
    int dhtPin = DHT_PIN;
    int dhtType = DHT22;
    int soilHumiditySensorPin = SOIL_HUMIDITY_PIN;
};

struct AppConfig {
    WiFiConfig wifi;
    MQTTConfig mqtt;
    GPIOControlConfig gpioControl;
    TimeConfig time;
    SensorConfig sensor;
};

extern AppConfig appConfig;

#endif
