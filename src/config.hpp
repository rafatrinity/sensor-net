#ifndef CONFIG_HPP
#define CONFIG_HPP

#define ECHO_PIN 25
#define TRIG_PIN 26
#define DHTPIN 4
#define DHTTYPE DHT22
#define BAUD 115200

#include <LiquidCrystal_I2C.h>
extern LiquidCrystal_I2C LCD;

#endif