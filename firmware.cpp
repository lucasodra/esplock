#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/base64.h>

using namespace websockets;

// Configuration
#define WIFI_SSID "Your_SSID"
#define WIFI_PASSWORD "Your_Password"
#define SERVER_ADDRESS "your-server.com"
#define PORT 443
#define DOOR_PIN 1
#define LED_PIN 16
#define NUMPIXELS 3

// Definitions
WiFiClientSecure wifiClient;
WebSocketsClient webSocketClient;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
Ticker keepAliveTicker;
Ticker reconnectTicker;
mbedtls_pk_context pk_ctx;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_entropy_context entropy;

void connectToWiFi();
void connectToWebSocket();
void handleWebSocketMessage(WsMessage message);
void sendKeepAlive();
void checkReconnect();
void setupNeoPixel();
void unlockDoor();
void lockDoor();
void rsaDecrypt(const char* input, size_t input_len, char* output, size_t* output_len);
void processCommand(const char* decryptedCommand);

void setup() {
    Serial.begin(115200);
    setupNeoPixel();
    lockDoor(); // Ensure door is locked on startup

    connectToWiFi();
    connectToWebSocket();

    // Initialize mbedTLS for RSA decryption
    mbedtls_pk_init(&pk_ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    const char *pers = "rsa_decrypt";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers));
    int ret = mbedtls_pk_parse_key(&pk_ctx, (const unsigned char *)your_private_key_pem, strlen(your_private_key_pem) + 1, NULL, 0);
    if (ret != 0) {
        Serial.println("Failed to load private key");
    }

    // Set up OTA updates
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();

    keepAliveTicker.attach(10, sendKeepAlive);
    reconnectTicker.attach(15, checkReconnect);
}

void loop() {
    ArduinoOTA.handle();
    webSocketClient.loop();
}

void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        blinkColor(strip.Color(0, 0, 255), 1, 500); // Blue blink for connecting
    }
    Serial.println("\nWiFi connected.");
    blinkColor(strip.Color(0, 255, 0), 3, 100); // Green blink for successful connection
}

void connectToWebSocket() {
    Serial.println("Connecting to WebSocket server...");
    webSocketClient.beginSSL(SERVER_ADDRESS, PORT, "/ws");
    webSocketClient.onEvent(handleWebSocketMessage);
    while (!webSocketClient.isConnected()) {
        webSocketClient.loop();
        Serial.println("Trying to connect to WebSocket...");
        blinkColor(strip.Color(255, 140, 0), 1, 500); // Orange blink for WebSocket connecting
        delay(1000);
    }
    Serial.println("WebSocket connected.");
    blinkColor(strip.Color(0, 255, 0), 5, 100); // Green blink for successful connection
}

void handleWebSocketMessage(WsMessage message) {
    Serial.println("Received message: " + message.data());
    blinkColor(strip.Color(255, 255, 255), 1, 100); // White blink for received message

    // Decrypt the command
    const char* encryptedCommand = message.data().c_str();
    size_t encryptedCommandLen = strlen(encryptedCommand);
    char decryptedCommand[512]; // Adjust size as needed
    size_t decryptedCommandLen = sizeof(decryptedCommand);
    
    rsaDecrypt(encryptedCommand, encryptedCommandLen, decryptedCommand, &decryptedCommandLen);
    decryptedCommand[decryptedCommandLen] = '\0'; // Null-terminate the decrypted string

    // Process the decrypted command
    processCommand(decryptedCommand);
}

void sendKeepAlive() {
    if (webSocketClient.isConnected()) {
        webSocketClient.send("keep_alive");
        Serial.println("Keep-alive sent");
        blinkColor(strip.Color(0, 0, 255), 1, 100); // Blue blink for keep-alive
    }
}

void checkReconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        connectToWiFi();
    }
    if (!webSocketClient.isConnected()) {
        Serial.println("WebSocket disconnected, reconnecting...");
        connectToWebSocket();
    }
}

void setupNeoPixel() {
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    pinMode(DOOR_PIN, OUTPUT);
    digitalWrite(DOOR_PIN, LOW); // Ensure door starts locked
}

void unlockDoor() {
    digitalWrite(DOOR_PIN, HIGH); // Unlock the door
    Serial.println("Door unlocked");
    blinkColor(strip.Color(0, 255, 0), 5, 100); // Green blink for door unlocked
}

void lockDoor() {
    digitalWrite(DOOR_PIN, LOW); // Lock the door
    Serial.println("Door locked");
    blinkColor(strip.Color(255, 0, 0), 5, 100); // Red blink for door locked
}

void blinkColor(uint32_t color, int blinks, int delayTime) {
    for (int i = 0; i < blinks; i++) {
        for (int j = 0; j < NUMPIXELS; j++) {
            strip.setPixelColor(j, color);
        }
        strip.show();
        delay(delayTime);
        for (int j = 0; j < NUMPIXELS; j++) {
            strip.setPixelColor(j, 0);
        }
        strip.show();
        delay(delayTime);
    }
}

void rsaDecrypt(const char* input, size_t input_len, char* output, size_t* output_len) {
    size_t olen = 0;
    unsigned char buf[512]; // Buffer for base64 decoded data
    size_t buf_len = 512;

    // Base64 decode the input
    int ret = mbedtls_base64_decode(buf, buf_len, &olen, (const unsigned char*)input, input_len);
    if (ret != 0) {
        Serial.println("Base64 decode failed");
        return;
    }

    // Decrypt the data
    ret = mbedtls_pk_decrypt(&pk_ctx, buf, olen, (unsigned char*)output, output_len, *output_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.println("RSA decryption failed");
    }
}

void processCommand(const char* decryptedCommand) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, decryptedCommand) != DeserializationError::Ok) {
        Serial.println("Failed to parse decrypted JSON!");
        return;
    }

    const char* command = doc["command"];
    const char* timestamp = doc["timestamp"];
    const char* password = doc["password"];

    // TODO: Validate the timestamp and password
    // Example check (implement actual validation logic):
    if (strcmp(command, "unlock") == 0) {
        unlockDoor();
    } else if (strcmp(command, "lock") == 0) {
        lockDoor();
    }
}
