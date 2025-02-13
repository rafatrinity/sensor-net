#include "sensorManager.hpp"
#include "network.hpp"
#include "config.hpp"
#include <vector>
#include <numeric>

DHT dht(DHTPIN, DHTTYPE);

void initializeSensors()
{
    dht.begin();
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

    // Debug: exibe os valores medidos e o target atual
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
        digitalWrite(2, HIGH);
        Serial.println("SSR activated: Humidity is below target.");
    }
    else if (humidity > target.airHumidity)
    {
        digitalWrite(2, LOW);
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
        int analogValue = 4095 - analogRead(34);
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
