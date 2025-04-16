#ifndef BOARD_ESP32_DEV_HPP
#define BOARD_ESP32_DEV_HPP

// --- Definições de Pinos para ESP32 DevKit V1 ---

// Pinos de Controle GPIO (Atuadores)
#define GPIO_HUMIDITY_PIN 2  // Pino para controlar umidificador/ventilador
#define GPIO_LIGHT_PIN 5     // Pino para controlar a iluminação

// Pinos de Sensores
#define DHT_PIN 4            // Pino de dados do sensor DHT22
#define SOIL_HUMIDITY_PIN 34 // Pino para o sensor de umidade do solo (ADC1_CH6)

// Pinos I2C para o LCD
#define SDA 33               // Pino SDA do I2C
#define SCL 32               // Pino SCL do I2C

// --- Configuração do Display LCD ---
#define LCD_I2C_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// --- Definições Específicas da Instalação/Board ---
#ifdef UNIT_TEST
    // --- Configurações para WOKWI / Teste ---
    #define MQTT_SERVER "mqtt.wokwi.com" // <--- ADICIONE/MODIFIQUE ESTA LINHA
    #define MQTT_PORT 1883               // <--- ADICIONE ESTA LINHA
    #define MQTT_ROOM_TOPIC "test"       // Identificador do cômodo/estufa para MQTT
    #define MQTT_CLIENT_ID "ESP32dev_test" // ID único do cliente MQTT para esta placa
#else
    // --- Configurações para Placa Real ---
    #define MQTT_SERVER "192.168.1.11"  // <--- ADICIONE/MODIFIQUE ESTA LINHA (Seu IP real)
    #define MQTT_PORT 1883              // <--- ADICIONE ESTA LINHA
    #define MQTT_ROOM_TOPIC "01"        // Identificador do cômodo/estufa real
    #define MQTT_CLIENT_ID "ESP32dev"   // ID único do cliente MQTT real
#endif

#endif // BOARD_ESP32_DEV_HPP