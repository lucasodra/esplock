#include <Adafruit_NeoPixel.h>
#include <ArduionoWebsockets.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define PIN 16      // Pin number for the NeoPixel
#define NUMPIXELS 1 // Number of NeoPixels in the strip

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

const char *ssid = "AnyBirdie";
const char *password = "PASSWORD";
const char *serverAddress = "192.168.1.107";
const char *serverName = "<OPENLOCK_API_ENDPOINT";

WebsocketsClient client;

// Code underneath verify whether unlock request
void onMessage(WebsocketsMessage message) {
    Serial.println("Received message from server: " + message);
    // Handle the message received from the server
}

void setup() {
    // put your setup code here, to run once:
    strip.begin();
    strip.show();

    Serial.begin(115200);
    colorWipe(strip.Color(255, 0, 0), 50); // Red
    delay(500);

    // Connecting to a WiFi Network -----------------------
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    colorWipe(strip.Color(0, 0, 255), 50); // Blue --> connected to wifi
    delay(500);

    // Connect to the WebSocket server -------------------
    client.connect(serverAddress, serverPort, "/");
    while (!client.available()) {
        client.poll();
        delay(10);
        colorWipe(strip.Color(233, 69, 233)); // Pink --> Connecting to server --> not connected to server
    }

    Serial.println("Connected to server");
    colorWipe(strip.Color(69, 233, 228)); // Cyan --> Connected to server

    // Set the message callback
    client.onMessage(onMessage);
}

int value = 0;

void loop() {
    // put your main code here, to run repeatedly:
    if (WiFi.status() == WL_CONNECTED) {
        HHTPClient http;
    }

    http.begin(serverName);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();

        Serial.println(httpResponseCode);
        Serial.println(response);

        if (response == "open") {
        }
    }

    client.poll();
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, int wait) {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
    }
}