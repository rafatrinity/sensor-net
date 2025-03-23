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
    configTime(appConfig.time.utcOffsetInSeconds, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Falha ao obter tempo NTP");
        return;
    }
    Serial.println("Tempo NTP sincronizado com sucesso!");
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

void lightControl(struct tm lightOn, struct tm lightOff, int gpioPin) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Erro ao obter horário atual!");
        return;
    }

    int now = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int start = lightOn.tm_hour * 60 + lightOn.tm_min;
    int end = lightOff.tm_hour * 60 + lightOff.tm_min;

    if ((start < end && now >= start && now < end) ||
        (start > end && (now >= start || now < end))) {
        digitalWrite(gpioPin, HIGH);
    } else {
        digitalWrite(gpioPin, LOW);
    }
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

    if (target.airHumidity == 0.0) {
        Serial.println("air humidity target not found");
        return humidity;
    }

    if (humidity < target.airHumidity)
    {
        digitalWrite(appConfig.gpioControl.humidityControlPin, HIGH);
    }
    else if (humidity > target.airHumidity)
    {
        digitalWrite(appConfig.gpioControl.humidityControlPin, LOW);
    }
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
