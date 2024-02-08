#include "Adafruit_NeoPixel.h"
#include "config.h"
#include "mbedtls/sha256.h"
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>

using namespace websockets;
// Create a WebSocket client instance
WebsocketsClient client;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    colorWipe(strip.Color(255, 0, 0), 50); // Red
    pinMode(DOOR_PIN, OUTPUT);
    digitalWrite(DOOR_PIN, HIGH);

    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    colorWipe(strip.Color(255, 255, 204), 50); // Light Yellow on Standby

    // Connect to the WebSocket server
    while (!client.connect(SERVERADDRESS, PORT, "/")) {
        Serial.println("Failed to connect to WebSocket server. Retrying...");
        delay(1000);
    }

    Serial.println("Connected to WebSocket server.");
    colorWipe(strip.Color(173, 216, 230), 50); // Light Blue connected

    // Register a callback function to handle incoming messages
    client.onMessage([](WebsocketsMessage message) {
        Serial.println("Received message: " + message.data());
        colorWipe(strip.Color(0, 0, 255), 50);   // Blue message received
        colorWipe(strip.Color(255, 125, 0), 50); // Orange connected, waiting for instruction

        // Parse JSON data here
        DynamicJsonDocument doc(1024); // Adjust size according to JSON size
        DeserializationError error = deserializeJson(doc, message.data());

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
        }

        // Extract from JSON
        const char *esp32ID = doc["esp32Code"];
        const char *pw = doc["password"];
        const char *instruction = doc["instruction"];
        colorWipe(strip.Color(75, 0, 130), 50); // Purple JSON parser success
        Serial.println(esp32ID);
        Serial.println(pw);
        Serial.println(instruction);

        // Check if receiving ID is the same as ESP's ID
        // Hash receiving password and hash ESP's password
        // Compare and check if correct:
        // Only if when both correct read instruction and execute

        if (strcmp(ESPID, esp32ID) == 0) { // ID is correct
            char ESPPWHash[65];
            hashPassword(ESPPW, ESPPWHash);

            char pwHash[65];
            hashPassword(pw, pwHash);

            // Compare hash
            if (strcmp(ESPPWHash, pwHash) == 0) {
                // INSERT UNLOCK CODE HERE
                Serial.println("UNLOCK");
                digitalWrite(DOOR_PIN, LOW);
                colorWipe(strip.Color(0, 255, 0), 50); // GREEN DOOR UNLOCK
            } else {
                // DON'T DO ANYTHING REJECT
                colorWipe(strip.Color(255, 0, 0), 50); // RED REJECT
            }
            digitalWrite(DOOR_PIN, HIGH);
            colorWipe(strip.Color(0, 0, 255), 50);
        }
    });
}

void loop() {
    // Handle WebSocket events
    if (client.available()) {
        client.poll();
    } else {
        while (!client.connect(SERVERADDRESS, PORT, "/")) {
            Serial.println("Failed to connect to WebSocket server. Retrying...");
            delay(1000);
        }
    }
    delay(500);

    // Your main loop code goes here
}

void hashPassword(const char *password, char *buffer) {
    unsigned char hash[32]; // Ensure this declaration is correct
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);                                                 // Corrected from start to starts
    mbedtls_sha256_update_ret(&ctx, (const unsigned char *)password, strlen(password)); // Use strlen instead of length
    mbedtls_sha256_finish_ret(&ctx, hash);                                              // Ensure hash is unsigned char[32]
    mbedtls_sha256_free(&ctx);

    for (int i = 0; i < 32; i++) {
        sprintf(&buffer[i * 2], "%02x", (unsigned int)hash[i]);
    }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, int wait) {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
    }
}