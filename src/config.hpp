#ifndef CONFIG_HPP
#define CONFIG_HPP
#define BAUD 115200

#include <LiquidCrystal_I2C.h>
#include <DHT.h>


extern LiquidCrystal_I2C LCD;


// ===============================================================
// WiFi Configuration
// ===============================================================
struct WiFiConfig {
    const char* ssid = "Casa";         // Default SSID
    const char* password = "12345678"; // Default Password
    // You can add more WiFi related settings here if needed,
    // like hostname, static IP configuration, etc.
};

// ===============================================================
// MQTT Configuration
// ===============================================================
struct MQTTConfig {
    const char* server = "192.168.1.11"; // Default MQTT Broker IP
    int port = 1883;                     // Default MQTT Broker Port
    const char* clientId = "ESP32Client1"; // Default MQTT Client ID
    const char* roomTopic = "01";         // Default Room Topic (e.g., for base topic "01/sensors")

    // You can add MQTT username/password, keep-alive settings, etc., if needed.
};

// ===============================================================
// GPIO Control Configuration
// ===============================================================
struct GPIOControlConfig {
    int humidityControlPin = 2;    // Pin for humidity control (SSR) - default pin 2
    int timeControlTestPin = 4;     // Example pin for time-based control testing - default pin 4
    // Add more GPIO pin configurations here as needed for different functionalities.
};

// ===============================================================
// Time Configuration (NTP)
// ===============================================================
struct TimeConfig {
    long utcOffsetInSeconds = -10800; // Default UTC Offset for Brasília (GMT-3)
    // You might add NTP server address configuration here if needed.
};

// ===============================================================
// Sensor Configuration (If you want to centralize sensor related settings)
// ===============================================================
struct SensorConfig {
    int dhtPin = 4;     // DHT sensor data pin - default pin 4 (might conflict with timeControlTestPin, adjust as needed!)
    int dhtType = DHT22; // DHT sensor type (DHT22, DHT11, DHT21)
    int soilHumiditySensorPin = 34; // Analog pin for soil humidity sensor - default pin 34

    // You can add configurations for other sensors here, like I2C addresses, etc.
};


// ===============================================================
// Application Configuration - Master struct to hold all configurations
// ===============================================================
struct AppConfig {
    WiFiConfig wifi;           // WiFi settings
    MQTTConfig mqtt;           // MQTT settings
    GPIOControlConfig gpioControl; // GPIO control settings
    TimeConfig time;           // Time (NTP) settings
    SensorConfig sensor;         // Sensor settings
};

// ===============================================================
// Global Configuration Instance (You can choose to use a global,
// or pass the config object around as needed)
// ===============================================================
extern AppConfig appConfig; // Declare the global instance

#endif