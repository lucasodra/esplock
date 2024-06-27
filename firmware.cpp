#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include <Ticker.h>
#include "mbedtls/sha256.h"

using namespace websockets;

// Configuration
#define WIFI_SSID "Anybirdie"
#define PASSWORD "@Anybirdie402*"
#define SERVERADDRESS "openlock-32cb74cf1b3e.herokuapp.com"
#define ESPID "anybirdie888"
#define ESPPW "GoodLuckForSoftLaunch"
#define PORT 3000
#define PIN 16
#define NUMPIXELS 3
#define DOOR_PIN 1

// Definitions
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
WebsocketsClient client;
Ticker dailyResetTicker;
Ticker keepAliveTicker;
Ticker wifiConnectTicker;

// Function Prototypes
void connectToWiFi();
void connectToWebSocket();
void handleMessage(WebsocketsMessage message);
void hashPassword(const char *password, char *buffer);
void colorWipe(uint32_t color, int wait);
void setupNeoPixel();
void dailyReset();
void lockDoorOnRestart();
void sendKeepAlive();

void setup() {
    Serial.begin(115200);
    setupNeoPixel();
    lockDoorOnRestart();

    colorWipe(strip.Color(255, 255, 255), 50); // Show white
    delay(500);
    connectToWiFi();
    delay(500);
    connectToWebSocket();
    delay(500);

    dailyResetTicker.attach(43200, dailyReset); // Set up a 12-hour daily reset
    keepAliveTicker.attach(5, sendKeepAlive);  // Set up a 5 second keep-alive message
    wifiConnectTicker.attach(60, connectToWiFi); // Setup up a 1-minute connect to wifi call
}

void loop() {
    // Handle WebSocket events
    client.poll();
}

void connectToWiFi() {
    colorWipe(strip.Color(255, 255, 255), 50); // Show white
    
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID, PASSWORD);
    }

    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        retryCount++;
        if (retryCount >= 20) { // Adjust the retry limit as needed
            Serial.println("\nExceeded maximum WiFi retries. Restarting...");
            colorWipe(strip.Color(255, 255, 0), 50); // Show orange
            delay(1000);
            ESP.restart();
        }
    }
    Serial.println("\nWiFi connected.");
    colorWipe(strip.Color(0, 255, 0), 50); // Show green
}

void connectToWebSocket() {
    colorWipe(strip.Color(255, 255, 255), 50); // Show white
    Serial.println("Connecting to WebSocket server...");
    String ws_url = String("ws://") + SERVERADDRESS + "/";
    Serial.println("Connecting to: " + ws_url);

    int retryCount = 0;
    while (!client.connect(ws_url)) {
        Serial.println("Failed to connect to WebSocket server. Retrying...");
        colorWipe(strip.Color(255, 255, 0), 50); // Show orange
        delay(1000);

        retryCount++;
        if (retryCount >= 10) {  // Adjust the retry limit as needed
            Serial.println("Exceeded maximum retries. Restarting...");
            colorWipe(strip.Color(255, 255, 0), 50); // Show orange
            delay(1000);
            ESP.restart();  // Restart the ESP32 if connection fails after several retries
        }
    }

    Serial.println("Connected to WebSocket server.");
    client.onMessage(handleMessage); // Set up message handler
    colorWipe(strip.Color(0, 255, 0), 50); // Show green
}

void handleMessage(WebsocketsMessage message) {
    Serial.println("Received message: " + message.data());
    colorWipe(strip.Color(255, 255, 255), 50); // White message received

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, message.data()) != DeserializationError::Ok) {
        Serial.println("Failed to parse JSON!");
        colorWipe(strip.Color(255, 0, 0), 50); // Red: access denied
        return;
    }

    const char *esp32ID = doc["esp32Code"];
    const char *pw = doc["password"];

    if (strcmp(ESPID, esp32ID) != 0) {
        Serial.println("ID mismatch!");
        colorWipe(strip.Color(255, 0, 0), 50); // Red: access denied
        return;
    }

    char ESPPWHash[65], pwHash[65];
    hashPassword(ESPPW, ESPPWHash);
    hashPassword(pw, pwHash);

    if (strcmp(ESPPWHash, pwHash) == 0) {
        digitalWrite(DOOR_PIN, HIGH); // Unlock the door
        colorWipe(strip.Color(0, 255, 0), 50); // Green: door unlocked
    } else {
        digitalWrite(DOOR_PIN, LOW); // Keep the door locked
        colorWipe(strip.Color(255, 0, 0), 50); // Red: access denied
    }

    delay(1000);
    digitalWrite(DOOR_PIN, LOW); // Keep the door locked
    colorWipe(strip.Color(255, 0, 0), 50); // Red: access denied
}

void hashPassword(const char *password, char *buffer) {
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char *)password, strlen(password));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    for (int i = 0; i < 32; i++) {
        sprintf(&buffer[i * 2], "%02x", (unsigned int)hash[i]);
    }
}


void colorWipe(uint32_t color, int wait) {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
    }
}

void setupNeoPixel() {
    strip.begin();
    colorWipe(strip.Color(255, 0, 0), 50); // Start with red color
    pinMode(DOOR_PIN, OUTPUT);
    digitalWrite(DOOR_PIN, LOW); // Initialize door as locked
}

void dailyReset() {
    Serial.println("Performing daily reset...");
    colorWipe(strip.Color(255, 165, 0), 50); // Show orange before reset
    delay(1000);
    ESP.restart();
}

void lockDoorOnRestart() {
    // Ensure the door remains locked on restart
    digitalWrite(DOOR_PIN, LOW);
    colorWipe(strip.Color(255, 0, 0), 50); // Red: door locked
}

void sendKeepAlive() {
    colorWipe(strip.Color(255, 255, 255), 50); // white
    if (client.available()) {
        client.send("keep-alive");
    }
    delay(1000);
    colorWipe(strip.Color(255, 0, 0), 50); // Red: door locked
}