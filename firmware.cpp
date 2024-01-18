#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#define PIN 16      // Pin number for the NeoPixel
#define NUMPIXELS 1 // Number of NeoPixels in the strip

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

AsyncWebServer server(PORT);
AsyncWebSocket ws("/ws");

void setup() {
    Serial.begin(115200);
    colorWipe(strip.Color(255, 0, 0), 50); // Red

    IPAddress ip_address(IP_1, IP_2, IP_3, IP_4);
    IPAddress gateway(GATEWAY_1, GATEWAY_2, GATEWAY_3, GATEWAY_4);
    IPAddress subnet(SUBNET_1, SUBNET_2, SUBNET_3, SUBNET_4);

    // Connect to Wi-Fi
    WiFi.config(ip_address, gateway, subnet);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
    colorWipe(strip.Color(255, 165, 0), 50); // Orange --> Connected to WiFi and on Standby

    // Handle WebSocket events
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.println("WebSocket client connected");
            colorWipe(strip.Color(0, 255, 0), 2000); // Green
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.println("WebSocket client disconnected");
            colorWipe(strip.Color(255, 165, 0), 2000); // Orange
        } else if (type == WS_EVT_DATA) {
            AwsFrameInfo *info = (AwsFrameInfo *)arg;
            if (info->opcode == WS_TEXT) {
                // Handle WebSocket text data
                String receivedData = String((char *)data);
                Serial.println("Received WebSocket data: " + receivedData);

                // Echo back to the client
                client->text(receivedData);
            }
        }
    });

    // Attach WebSocket to the server
    server.addHandler(&ws);

    // Start server
    server.begin();
}

void loop() {
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