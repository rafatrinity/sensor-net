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

// --- Definições Específicas da Instalação/Board ---
// (Podem variar dependendo de como você quer identificar esta unidade)
#define MQTT_ROOM_TOPIC "01"       // Identificador do cômodo/estufa para MQTT
#define MQTT_CLIENT_ID "ESP32Client1" // ID único do cliente MQTT para esta placa

#endif // BOARD_ESP32_DEV_HPP