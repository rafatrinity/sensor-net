#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>

#define ECHO_PIN 25
#define TRIG_PIN 26
#define DHTPIN 4
#define DHTTYPE DHT22

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *mqtt_server = "172.17.0.4";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password, 6);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  dht.begin();
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("OK! IP=");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
}

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  int duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.0342 / 2;
}

float readTemperature() {
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return -999.0;
  }
  return temperature;
}

float readHumidity() {
  float humidity = dht.readHumidity();
  if (isnan(humidity)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return -999.0;
  }
  return humidity;
}


void loop()
{
  if (!client.connected())
  {
    while (!client.connected())
    {
      Serial.println("Reconnecting to MQTT...");
      if (client.connect("ESP32Client"))
      {
        Serial.println("reconnected");
      }
      else
      {
        Serial.print("failed with state ");
        Serial.print(client.state());
        delay(2000);
      }
    }
  }
  
  float percentage = (402 - readDistanceCM())*0.25;
  char strPercentage[50];
  sprintf(strPercentage, "%.2f", percentage);
  client.publish("01/water_level", strPercentage, true);

  float temperature = readTemperature();
  char strTemperature[50];
  sprintf(strTemperature, "%.2f", temperature);
  client.publish("01/temperature", strTemperature, true);

  float airHumidity = readHumidity();
  char strAirHumidity[50];
  sprintf(strAirHumidity, "%.2f", airHumidity);
  client.publish("01/air_humidity", strAirHumidity, true);
  
  float Ph = analogRead(35) * 0.003418803;
  char strPh[50];
  sprintf(strPh, "%.2f", Ph);
  client.publish("01/ph", strPh, true);

  float soilHumidity = analogRead(34) * 0.024420024;
  char strSoilHumidity[50];
  sprintf(strSoilHumidity, "%.2f", soilHumidity);
  client.publish("01/soil_humidity", strSoilHumidity, true);
  
  delay(3000);
}
