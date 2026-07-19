#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h> // Handles local HTTP queries
#include <ESP8266HTTPClient.h> // Handles pushing data to ThingSpeak
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include "DHT.h"

// Wi-Fi Credentials
const char* ssid = ""; // insert your wifi SSID
const char* password = ""; // insert your wifi password

// ThingSpeak Setup
const char* serverName = "http://api.thingspeak.com/update"; 
const char* writeApiKey = "";  // insert your apikey here

// DHT11 Setup
#define DHTPIN D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Initialize the local Web Server on port 80
ESP8266WebServer server(80);

// Global variables to store the latest sensor readings
float humidity = 0.0;
float temperature = 0.0;

// Dual timers for cloud pushing and reading sensors
unsigned long prevCloudMillis = 0;
unsigned long prevSensorMillis = 0;

// --- TIMING TUNING ---
const long cloudInterval = 900000; // 15 minutes in milliseconds (15 * 60 * 1000)
const long sensorInterval = 2000;  // Read local sensor every 2 seconds

// --- LOCAL WEB SERVER HANDLERS ---

void handleRoot() {
  String message = "NodeMCU Dual Broadcast Node is running!\n\n";
  message += "Local endpoint: http://" + WiFi.localIP().toString() + "/temp\n";
  message += "Cloud Broadcast Rate: Once per 15 minutes\n";
  server.send(200, "text/plain", message);
}

void handleTemp() {
  if (isnan(humidity) || isnan(temperature)) {
    server.send(500, "application/json", "{\"error\": \"Failed to read from sensor\"}");
    return;
  }

  String jsonResponse = "{";
  jsonResponse += "\"temperature_celsius\":" + String(temperature, 1) + ",";
  jsonResponse += "\"humidity_percent\":" + String(humidity, 1);
  jsonResponse += "}";

  server.send(200, "application/json", jsonResponse);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting Hybrid Data Node (15-min Cloud Interval)...");
  
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Setup OTA
  ArduinoOTA.setHostname("my-nodemcu-dht");
  ArduinoOTA.begin();

  // Configure Local HTTP Endpoints
  server.on("/", handleRoot);
  server.on("/temp", handleTemp);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("Local HTTP Server Started");
  Serial.print("Local IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();   // Keep OTA listening
  server.handleClient(); // Instantly handle local app queries

  unsigned long currentMillis = millis();

  // TIMER 1: Update local data every 2 seconds for high local responsiveness
  if (currentMillis - prevSensorMillis >= sensorInterval) {
    prevSensorMillis = currentMillis;
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temperature = t;
    }
  }

  // TIMER 2: Pushes data to ThingSpeak cloud only once every 15 minutes
  if (currentMillis - prevCloudMillis >= cloudInterval) {
    prevCloudMillis = currentMillis;

    // Skip cloud transmission if readings are currently faulty
    if (isnan(humidity) || isnan(temperature)) return;

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;
      
      String url = String(serverName) + "?api_key=" + String(writeApiKey) + 
                   "&field1=" + String(temperature, 1) + 
                   "&field2=" + String(humidity, 1);
      
      http.begin(client, url);
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.printf("[Cloud] 15-Min Update Success. Status code: %d\n", httpResponseCode);
      } else {
        Serial.printf("[Cloud] Error failed to push. Error code: %s\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();
    } else {
      Serial.println("WiFi dropped. Reconnecting...");
      WiFi.begin(ssid, password);
    }
  }
}