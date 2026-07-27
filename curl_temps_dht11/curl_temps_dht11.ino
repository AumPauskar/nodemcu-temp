#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include "DHT.h"
#include "secrets.h"

// DHT11 Configuration
#define DHTPIN D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

// Globals & Timers
float humidity = 0;
float temperature = 0;

unsigned long prevUploadMillis = 0;
unsigned long prevSensorMillis = 0;

// Intervals
const unsigned long SENSOR_INTERVAL = 2000;                      // Read sensor every 2 seconds
const unsigned long UPLOAD_INTERVAL = UPLOAD_INTERVAL_MIN * 60000UL; // Convert minutes from secrets.h to ms

void handleStatus() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Not found. Use /status\"}");
}

void handleForceThingSpeakUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "application/json", "{\"error\":\"Wi-Fi is not connected\"}");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String thingspeakUrl = String(THINGSPEAK_SERVER) +
                         "?api_key=" + String(THINGSPEAK_API_KEY) +
                         "&field1=" + String(temperature, 1) +
                         "&field2=" + String(humidity, 1);

  http.begin(client, thingspeakUrl);
  int httpCode = http.GET();
  String entryId = httpCode > 0 ? http.getString() : "";
  http.end();

  // ThingSpeak returns HTTP 200 with body "0" when it does not accept an update.
  if (httpCode == HTTP_CODE_OK && entryId != "0" && entryId.length() > 0) {
    Serial.printf("Forced ThingSpeak update accepted (entry %s)\n", entryId.c_str());
    server.send(200, "application/json", "{\"success\":true,\"entry_id\":" + entryId + "}");
  } else {
    Serial.printf("Forced ThingSpeak update rejected (HTTP %d, response: %s)\n", httpCode, entryId.c_str());
    server.send(429, "application/json", "{\"success\":false,\"http_code\":" + String(httpCode) + ",\"response\":\"" + entryId + "\"}");
  }
}

void uploadData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected. Skipping upload.");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  // ---------------- 1. Send to ThingSpeak ----------------
  String thingspeakUrl = String(THINGSPEAK_SERVER) +
                         "?api_key=" + String(THINGSPEAK_API_KEY) +
                         "&field1=" + String(temperature, 1) +
                         "&field2=" + String(humidity, 1);

  http.begin(client, thingspeakUrl);
  int tsCode = http.GET();
  if (tsCode > 0) {
    Serial.printf("ThingSpeak Upload OK (%d)\n", tsCode);
  } else {
    Serial.printf("ThingSpeak Error: %s\n", http.errorToString(tsCode).c_str());
  }
  http.end();

  // ---------------- 2. Send to Local Server ----------------
  // Sends a standard HTTP POST request formatted as JSON
  http.begin(client, LOCAL_SERVER_ENDPOINT);
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{";
  jsonPayload += "\"temperature\":" + String(temperature, 1) + ",";
  jsonPayload += "\"humidity\":" + String(humidity, 1);
  jsonPayload += "}";

  int localCode = http.POST(jsonPayload);
  if (localCode > 0) {
    Serial.printf("Local Server Upload OK (%d)\n", localCode);
  } else {
    Serial.printf("Local Server Error: %s\n", http.errorToString(localCode).c_str());
  }
  http.end();
}

void setup() {
  Serial.begin(115200);

  // Initialize DHT Sensor
  dht.begin();

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure Arduino OTA
  ArduinoOTA.setHostname("nodemcu-weather-station");
  ArduinoOTA.begin();

  // Exposes the latest sensor readings at http://<device-ip>/status.
  server.on("/", HTTP_GET, handleStatus);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/update", HTTP_GET, handleForceThingSpeakUpdate);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started. Open http://" + WiFi.localIP().toString() + "/status");
}

void loop() {
  // Always handle OTA requests in background
  ArduinoOTA.handle();
  server.handleClient();

  unsigned long currentMillis = millis();

  // Read DHT sensor periodically
  if (currentMillis - prevSensorMillis >= SENSOR_INTERVAL) {
    prevSensorMillis = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h)) humidity = h;
    if (!isnan(t)) temperature = t;
  }

  // Upload data periodically
  if (currentMillis - prevUploadMillis >= UPLOAD_INTERVAL) {
    prevUploadMillis = currentMillis;

    Serial.println("---------------------------------");
    Serial.printf("Temp: %.1f °C | Humidity: %.1f %%\n", temperature, humidity);
    uploadData();
    Serial.println("---------------------------------");
  }
}
