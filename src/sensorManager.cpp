#include "sensorManager.hpp"
#include "network.hpp"
#include "config.hpp"
#include <vector>
#include <numeric>
#include <Arduino.h>
#include <WiFi.h>          
#include <WiFiUdp.h>       
#include <NTPClient.h>     

DHT dht(appConfig.sensor.dhtPin, appConfig.sensor.dhtType);


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);



void initializeNTP() {
    timeClient.begin();
    timeClient.setTimeOffset(appConfig.time.utcOffsetInSeconds);
    Serial.println("NTP Client initialized.");
}


int getCurrentHour() {
    if (!timeClient.update()) {
        Serial.println("Failed to get NTP time!");
        return -1; 
    }
    return timeClient.getHours();
}

void initializeSensors()
{
    dht.begin();
}

void lightControl(int horaIni, int horaFim, int gpioPin) {
    int currentHour = getCurrentHour(); 

    
    if (horaIni < 0 || horaIni > 23 || horaFim < 0 || horaFim > 23) {
        Serial.println("Horas inválidas! Use horas entre 0 e 23.");
        return; 
    }

    Serial.print("Hora atual: ");
    Serial.print(currentHour);
    Serial.print(" - Intervalo: ");
    Serial.print(horaIni);
    Serial.print("h - ");
    Serial.print(horaFim);
    Serial.print("h.  Pino GPIO: ");
    Serial.print(gpioPin);


    if (horaIni < horaFim) { 
        if (currentHour >= horaIni && currentHour < horaFim) {
            digitalWrite(gpioPin, HIGH); 
            Serial.println(" - ATIVADO");
        } else {
            digitalWrite(gpioPin, LOW);  
            Serial.println(" - DESATIVADO");
        }
    } else { 
        if (currentHour >= horaIni || currentHour < horaFim) {
            digitalWrite(gpioPin, HIGH); 
            Serial.println(" - ATIVADO");
        } else {
            digitalWrite(gpioPin, LOW);  
            Serial.println(" - DESATIVADO");
        }
    }
    delay(1000); 
}

float readTemperature()
{
    float temperature = dht.readTemperature();
    if (isnan(temperature))
    {
        Serial.println(F("Failed to read from DHT sensor!"));
        return -999.0;
    }
    return temperature;
}

float readHumidity()
{
    float humidity = dht.readHumidity();
    if (isnan(humidity))
    {
        Serial.println(F("Failed to read from DHT sensor!"));
        return -999.0;
    }

    
    Serial.println("----- readHumidity() -----");
    Serial.print("Measured humidity: ");
    Serial.println(humidity);
    Serial.print("Current target.airHumidity: ");
    Serial.println(target.airHumidity);

    if (target.airHumidity == 0.0) {
        Serial.println("air humidity target not found");
        return humidity;
    }

    if (humidity < target.airHumidity)
    {
        digitalWrite(appConfig.gpioControl.humidityControlPin, HIGH);
        Serial.println("SSR activated: Humidity is below target.");
    }
    else if (humidity > target.airHumidity)
    {
        digitalWrite(appConfig.gpioControl.humidityControlPin, LOW);
        Serial.println("SSR deactivated: Humidity is above target.");
    }
    Serial.println("--------------------------");
    return humidity;
}

float readSoilHumidity()
{
    std::vector<int> arr;

    for (int i = 0; i < 100; i++)
    {
        int analogValue = 4095 - analogRead(appConfig.sensor.soilHumiditySensorPin);
        if (analogValue > 0)
        {
            arr.push_back(analogValue);
        }
    }

    if (!arr.empty())
    {
        float sum = std::accumulate(arr.begin(), arr.end(), 0);
        float average = sum / arr.size();
        return average / 40.95;
    }
    else
    {
        return 0.0;
    }
}

float calculateVpd(float tem, float hum)
{
    if(tem == -999 || hum == -999) return NAN;
    float es = 6.112 * exp((17.67 * tem) / (tem + 243.5));
    float ea = (hum / 100) * es;
    float vpd = es - ea;
    return vpd/10;
}
