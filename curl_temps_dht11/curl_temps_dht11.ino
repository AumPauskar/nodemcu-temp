#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "DHT.h"
#include "secrets.h" // Stores Wi-Fi & API Credentials

// -------------------- Configuration --------------------
// Sleep duration in microseconds: 30 minutes = 30 * 60 * 1,000,000 us
const uint64_t DEEP_SLEEP_TIME = 30ULL * 60ULL * 1000000ULL; 

// -------------------- DHT11 --------------------
#define DHTPIN D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------------------- Battery --------------------
#define BATTERY_PIN A0
const float DIVIDER_RATIO = (120.0 + 47.0) / 47.0;
const float ADC_MAX_VOLTAGE = 3.30;

// -------------------- Functions --------------------

float readBatteryVoltage() {
  long total = 0;
  for (int i = 0; i < 20; i++) {
    total += analogRead(BATTERY_PIN);
    delay(2);
  }
  float raw = total / 20.0;
  float adcVoltage = raw * (ADC_MAX_VOLTAGE / 1023.0);
  return adcVoltage * DIVIDER_RATIO;
}

int getBatteryPercent(float voltage) {
  if (voltage >= 8.40) return 100;
  if (voltage >= 8.30) return 95;
  if (voltage >= 8.20) return 90;
  if (voltage >= 8.10) return 85;
  if (voltage >= 8.00) return 80;
  if (voltage >= 7.90) return 72;
  if (voltage >= 7.80) return 65;
  if (voltage >= 7.70) return 58;
  if (voltage >= 7.60) return 50;
  if (voltage >= 7.50) return 42;
  if (voltage >= 7.40) return 35;
  if (voltage >= 7.30) return 28;
  if (voltage >= 7.20) return 20;
  if (voltage >= 7.10) return 15;
  if (voltage >= 7.00) return 10;
  if (voltage >= 6.80) return 5;
  return 0;
}

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize DHT Sensor
  dht.begin();
  delay(100); // Give sensor a moment to initialize

  // 2. Read Sensors immediately
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  float batteryVoltage = readBatteryVoltage();
  int batteryPercent = getBatteryPercent(batteryVoltage);

  // If reading fails, retry up to 3 times
  int retries = 0;
  while ((isnan(humidity) || isnan(temperature)) && retries < 3) {
    delay(1000);
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    retries++;
  }

  // 3. Connect to Wi-Fi with a timeout
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  int wifiRetries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) {
    delay(500);
    Serial.print(".");
    wifiRetries++;
  }

  // 4. Send Data to ThingSpeak if Wi-Fi connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected. Uploading data...");
    
    WiFiClient client;
    HTTPClient http;

    String url = String(THINGSPEAK_SERVER) +
                 "?api_key=" + String(THINGSPEAK_API_KEY) +
                 "&field1=" + String(temperature, 1) +
                 "&field2=" + String(humidity, 1) +
                 "&field3=" + String(batteryVoltage, 2) +
                 "&field4=" + String(batteryPercent);

    http.begin(client, url);
    int code = http.GET();

    if (code > 0) {
      Serial.printf("ThingSpeak OK (%d)\n", code);
    } else {
      Serial.println(http.errorToString(code));
    }
    http.end();
  } else {
    Serial.println("\nWiFi connection failed. Skipping upload.");
  }

  // 5. Enter Deep Sleep
  Serial.println("Entering Deep Sleep for 30 minutes...");
  ESP.deepSleep(DEEP_SLEEP_TIME);
}

void loop() {
  // Deep sleep resets the board every cycle, so loop() is never executed.
}