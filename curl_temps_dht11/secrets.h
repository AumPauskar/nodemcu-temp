#ifndef SECRETS_H
#define SECRETS_H

// Wi-Fi Credentials
const char* SECRET_SSID = "";
const char* SECRET_PASS = "";

// ThingSpeak Configuration
const char* THINGSPEAK_SERVER  = "http://api.thingspeak.com/update";
const char* THINGSPEAK_API_KEY = "";

// Local Server Configuration (HTTP POST target)
const char* LOCAL_SERVER_ENDPOINT = "http://192.168.1.100:5000/api/data"; 

// Data upload interval in minutes (e.g., 30 for every 30 mins)
const unsigned long UPLOAD_INTERVAL_MIN = 30;

#endif