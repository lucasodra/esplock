#include "Adafruit_NeoPixel.h"
#include "config.h"
#include <ArduinoWebsockets.h>
#include <WiFi.h>

#define PIN 16
#define NUMPIXELS 3

using namespace websockets;

// Create a WebSocket client instance
WebsocketsClient client;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    colorWipe(strip.Color(255, 0, 0), 50); // Red

    // Connect to Wi-Fi
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    colorWipe(strip.Color(255, 125, 0), 50); // Orange on Standby

    // Connect to the WebSocket server
    while (!client.connect(SERVERADDRESS, PORT, "/")) {
        Serial.println("Failed to connect to WebSocket server. Retrying...");
        delay(1000);
    }

    Serial.println("Connected to WebSocket server.");
    colorWipe(strip.Color(0, 255, 0), 50); // Green connected

    // Register a callback function to handle incoming messages
    client.onMessage([](WebsocketsMessage message) {
        Serial.println("Received message: " + message.data());
        colorWipe(strip.Color(0, 0, 255), 2000); // Blue message received
        colorWipe(strip.Color(0, 255, 0), 50);   // Green connected, waiting for instruction
    });
}

void loop() {
    // Handle WebSocket events
    if (client.available()) {
        client.poll();
    }
    delay(500);

    // Your main loop code goes here
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, int wait) {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
    }
}