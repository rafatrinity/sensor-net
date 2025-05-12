#ifndef BOARD_XIAO_C3_HPP
#define BOARD_XIAO_C3_HPP

// --- Definições de Pinos para Seeed Studio XIAO ESP32-C3 ---

// Pinos de Controle GPIO (Atuadores)
// NOTE: Verifique a pinagem exata do XIAO C3 e quais pinos são seguros para usar como output.
//       Pinos como D0, D1 podem ter restrições. D4, D5 são geralmente seguros.
#define GPIO_HUMIDITY_PIN 0 // (Antigo D0 - verificar se é adequado)
#define GPIO_LIGHT_PIN 4    // (D4)

// Pinos de Sensores
#define DHT_PIN 10           // (D10) Pino de dados do sensor DHT22
#define SOIL_HUMIDITY_PIN 2 // (D2 - ADC1_CH2) Pino para o sensor de umidade do solo

// Define o pino do botão para ativar o modo de pareamento
#define PAIRING_BUTTON_PIN 1


// Pinos I2C para o LCD
// No XIAO ESP32-C3, os pinos padrão para I2C são:
#define SDA 6                // (D6) Pino SDA do I2C
#define SCL 7                // (D7) Pino SCL do I2C

// --- Configuração do Display LCD/OLED ---
// Verifique o endereço do seu display! 0x3C ou 0x3D são comuns para OLEDs. 0x27 para LCDs.
#define LCD_I2C_ADDR 0x3C    // Exemplo para OLED comum no XIAO C3 (VERIFIQUE O SEU!)
#define LCD_COLS 16          // Ajuste se usar OLED maior/diferente
#define LCD_ROWS 2           // Ajuste se usar OLED maior/diferente

// --- Definições Específicas da Instalação/Board ---
// (Podem variar dependendo de como você quer identificar esta unidade)
// --- Definições Específicas da Instalação/Board ---
#ifdef UNIT_TEST
    // --- Configurações para WOKWI / Teste ---
    #define MQTT_SERVER "mqtt.wokwi.com" // <--- ADICIONE/MODIFIQUE ESTA LINHA
    #define MQTT_PORT 1883               // <--- ADICIONE ESTA LINHA
    #define MQTT_ROOM_TOPIC "test"       // Identificador do cômodo/estufa para MQTT
    #define MQTT_CLIENT_ID "ESP-xiao-C3_test" // ID único do cliente MQTT para esta placa
#else
    // --- Configurações para Placa Real ---
    #define MQTT_SERVER "192.168.1.11"  // <--- ADICIONE/MODIFIQUE ESTA LINHA (Seu IP real)
    #define MQTT_PORT 1883              // <--- ADICIONE ESTA LINHA
    #define MQTT_ROOM_TOPIC "01"        // Identificador do cômodo/estufa real
    #define MQTT_CLIENT_ID "ESP-xiao-C3"   // ID único do cliente MQTT real
#endif
#endif // BOARD_XIAO_C3_HPP