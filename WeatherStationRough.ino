#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_BMP085.h>
#include "Arduino_LED_Matrix.h"

// Define Pins and Parameters
#define DHTPIN 2                    
#define DHTTYPE DHT22               
#define SOUND_PIN A0                
#define LED_PIN 12                  
#define LED_PIN_MEASURE 11          
#define RaindropSensor1 A1         
#define RaindropSensor2 A2         

// Rain threshold and descriptions
int rainThresholds[] = {300, 375, 450, 525, 600, 675, 750, 825, 900};
const char* rainDescriptions[] = {
  "100% Rain; Thunderstorm",  // FULL WATER DIVE
  "87.5% Rain; Stormy Rain",
  "75% Rain; Heavy Rain",
  "62.5% Rain; Moderate-Heavy Rain",
  "50% Rain; Moderate Rain",
  "37.5% Rain; Moderate-Light Rain",
  "25% Rain; Light Rain",
  "12.5% Rain; Minimal Rain",
  "0% Rain; No Rain"  // COMPLETELY DRY
};

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
Adafruit_BMP085 bmp;

// Wi-Fi credentials
const char* ssid = "xxxx";
const char* password = "xxxx";

// MQTT Naming
String topicPre = "xxxx";

// Mosquitto broker details
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = xxxx;

WiFiClient wifiClient;
PubSubClient client(wifiClient);
ArduinoLEDMatrix matrix;

void blinkLED(int duration_ms) {
  unsigned long start_time = millis();
  bool led_on = false;

  while (millis() - start_time < duration_ms) {
    digitalWrite(LED_PIN, led_on ? HIGH : LOW);
    led_on = !led_on;
    delay(500);
  }
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);

  // Initialize I2C communication
  Wire.begin();
  delay(100);

  // Initialize BH1750 sensor with debug check
  if (lightMeter.begin()) {
    Serial.println(F("BH1750 Initialized"));
  } else {
    Serial.println(F("Failed to initialize BH1750 sensor!"));
    while (1);
  }

  delay(100);

  // Initialize BMP180 sensor with debug check
  if (bmp.begin()) {
    Serial.println(F("BMP180 Initialized"));
  } else {
    Serial.println(F("Failed to initialize BMP180 sensor!"));
    while (1);
  }

  delay(2000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Wi-Fi connected");

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ArduinoClient")) {
      Serial.println("connected");
      client.subscribe("test/topic");
      blinkLED(5000);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
      return;
    }
  }

  client.loop();

  if (client.connected()) {
    // Read temperature and humidity from the DHT22 sensor
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Read light level from BH1750
    float lux = lightMeter.readLightLevel();
    if (lux < 0) {
      Serial.println("Failed to read from BH1750 sensor!");
      return;
    }

    // Rain Sensor Reading
    int sensor1Value = analogRead(RaindropSensor1);  // Read from analog pin A1
    int sensor2Value = analogRead(RaindropSensor2);  // Read from analog pin A2
    int combinedValue = (sensor1Value + sensor2Value) / 2;  // Average of the two readings

    // Default to "No Rain" if no thresholds are met
    const char* rainCondition = rainDescriptions[8];

    // Determine rain condition based on thresholds
    for (int i = 0; i < 9; i++) {
      if (combinedValue < rainThresholds[i]) {
        rainCondition = rainDescriptions[i];
        break;
      }
    }

    Serial.println(rainCondition);  // Print the matching rain condition to serial monitor

    // Read sound level from KY-037
    int soundLevel = analogRead(SOUND_PIN);
    Serial.println(soundLevel);

    // Read pressure from BMP180
    float bmpPressure = bmp.readPressure() / 100.0;

    if (isnan(bmpPressure)) {
      Serial.println("Failed to read from BMP180 sensor!");
      return;
    }

    // Create the message strings
    String humidityStr = String(h);
    String temperatureStr = String(t);
    String soundLevelStr = String(soundLevel);
    String luxStr = String(lux);
    String bmpPressureStr = String(bmpPressure);
    String rainConditionStr = String(rainCondition);

    // Publish the data to the MQTT broker
    if (client.publish((topicPre + "/temp").c_str(), temperatureStr.c_str())) {
      Serial.println("Temperature sent: " + temperatureStr);
    } else {
      Serial.println("Failed to send temperature");
    }

    if (client.publish((topicPre + "/humidity").c_str(), humidityStr.c_str())) {
      Serial.println("Humidity sent: " + humidityStr);
    } else {
      Serial.println("Failed to send humidity");
    }

    if (client.publish((topicPre + "/sound").c_str(), soundLevelStr.c_str())) {
      Serial.println("Sound level sent: " + soundLevelStr);
    } else {
      Serial.println("Failed to send sound level");
    }

    if (client.publish((topicPre + "/light").c_str(), luxStr.c_str())) {
      Serial.println("Light level sent: " + luxStr);
    } else {
      Serial.println("Failed to send light level");
    }

    if (client.publish((topicPre + "/bmpPressure").c_str(), bmpPressureStr.c_str())) {
      Serial.println("BMP180 Pressure sent: " + bmpPressureStr);
    } else {
      Serial.println("Failed to send BMP180 pressure");
    }

    if (client.publish((topicPre + "/rainCondition").c_str(), rainConditionStr.c_str())) {
      Serial.println("Rain condition sent: " + rainConditionStr);
    } else {
      Serial.println("Failed to send rain condition");
    }

    delay(5000);
  }
}