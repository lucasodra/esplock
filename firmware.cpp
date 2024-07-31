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
#include <Preferences.h>

using namespace websockets;

// Configuration for NeoPixel LED
#define LED_PIN 16
#define NUMPIXELS 3

// User-Preset Values
#define DOOR_ID "your_door_id_here"               // Set your Door ID
#define INITIAL_WIFI_SSID "your_initial_ssid"     // Set initial WiFi SSID
#define INITIAL_WIFI_PASSWORD "your_initial_password" // Set initial WiFi password
#define INITIAL_SERVER_ADDRESS "your_server_address"  // Set the OpenLock server address
#define INITIAL_PRESET_PASSWORD "your_secure_password" // Set the initial preset password

// Globals for NeoPixel, WiFi, WebSocket, and Crypto
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClientSecure wifiClient;
WebSocketsClient webSocketClient;
Ticker keepAliveTicker;
Ticker reconnectTicker;
mbedtls_pk_context pk_ctx;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_entropy_context entropy;
Preferences preferences;

// Global Variables for Settings
String wifiSSID = INITIAL_WIFI_SSID;
String wifiPassword = INITIAL_WIFI_PASSWORD;
String serverAddress = INITIAL_SERVER_ADDRESS;
String presetPassword = INITIAL_PRESET_PASSWORD;

// Function Prototypes
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
void generateAndStoreKeyPair();
bool loadKeyPair();
void saveWiFiCredentials(const String &ssid, const String &password);
void saveServerAddress(const String &address);
void savePresetPassword(const String &password);
void loadStoredData();
void sendPublicKey();
void sendStatus();
bool validatePresetPassword(const char* providedPassword);
bool validateTimestamp(const char* timestamp);

void setup() {
    Serial.begin(115200);
    setupNeoPixel();
    lockDoor(); // Ensure door is locked on startup

    preferences.begin("crypto_keys", false);
    loadStoredData();

    connectToWiFi();
    connectToWebSocket();

    // Initialize mbedTLS for RSA key management
    mbedtls_pk_init(&pk_ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    const char *pers = "rsa_genkey";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers));

    // Load or generate key pair
    if (!loadKeyPair()) {
        generateAndStoreKeyPair();
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
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
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
    webSocketClient.beginSSL(serverAddress.c_str(), PORT, "/ws");
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

    // Decrypt and process the command
    char decryptedCommand[512]; // Adjust size as needed
    size_t decryptedCommandLen = sizeof(decryptedCommand);
    rsaDecrypt(message.data().c_str(), strlen(message.data().c_str()), decryptedCommand, &decryptedCommandLen);
    decryptedCommand[decryptedCommandLen] = '\0'; // Null-terminate the decrypted string

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

    // Validate the password
    if (!validatePresetPassword(password)) {
        Serial.println("Invalid password");
        blinkColor(strip.Color(255, 0, 0), 5, 100); // Red blink for invalid password
        return;
    }

    // Validate the timestamp
    if (!validateTimestamp(timestamp)) {
        Serial.println("Invalid or outdated timestamp");
        blinkColor(strip.Color(255, 0, 0), 5, 100); // Red blink for invalid timestamp
        return;
    }

    // Process the command
    if (strcmp(command, "unlock") == 0) {
        unlockDoor();
    } else if (strcmp(command, "lock") == 0) {
        lockDoor();
    } else if (strcmp(command, "updateWifi") == 0) {
        const char* newSSID = doc["newSSID"];
        const char* newPassword = doc["newPassword"];
        saveWiFiCredentials(newSSID, newPassword);
        ESP.restart();
    } else if (strcmp(command, "updateServer") == 0) {
        const char* newServerAddress = doc["newServerAddress"];
        saveServerAddress(newServerAddress);
        ESP.restart();
    } else if (strcmp(command, "updatePresetPassword") == 0) {
        const char* newPresetPassword = doc["newPresetPassword"];
        savePresetPassword(newPresetPassword);
    } else if (strcmp(command, "getPublicKey") == 0) {
        sendPublicKey();
    } else if (strcmp(command, "getStatus") == 0) {
        sendStatus();
    } else {
        Serial.println("Unknown command");
        blinkColor(strip.Color(255, 255, 0), 5, 100); // Yellow blink for unknown command
    }
}

bool validatePresetPassword(const char* providedPassword) {
    return strcmp(providedPassword, presetPassword.c_str()) == 0;
}

bool validateTimestamp(const char* timestamp) {
    // Example: Implement the logic to validate the timestamp
    // This could include checking if the timestamp is within a certain range
    // from the current time to prevent replay attacks.

    // Convert the timestamp to a time structure
    struct tm tm;
    if (strptime(timestamp, "%Y-%m-%dT%H:%M:%S", &tm) == NULL) {
        return false; // Timestamp format is invalid
    }

    // Convert the time structure to a time_t (epoch time)
    time_t messageTime = mktime(&tm);

    // Get the current time
    time_t currentTime = time(NULL);

    // Define an acceptable time difference (e.g., 5 minutes)
    const time_t allowedTimeDifference = 5 * 60; // 5 minutes in seconds

    // Check if the message time is within the acceptable range
    if (abs(currentTime - messageTime) > allowedTimeDifference) {
        return false; // Timestamp is too far from the current time
    }

    return true; // Timestamp is valid
}

void generateAndStoreKeyPair() {
    int ret = mbedtls_pk_setup(&pk_ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        Serial.println("Failed to setup pk_ctx");
        return;
    }

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk_ctx), mbedtls_ctr_drbg_random, &ctr_drbg, 2048, 65537);
    if (ret != 0) {
        Serial.println("Failed to generate RSA key pair");
        return;
    }

    // Extract and store public key
    unsigned char publicKey[1600];
    ret = mbedtls_pk_write_pubkey_pem(&pk_ctx, publicKey, sizeof(publicKey));
    if (ret == 0) {
        preferences.putString("publicKey", (const char *)publicKey);
        Serial.println("Stored public key in NVS");
    } else {
        Serial.println("Failed to write public key to PEM format");
    }

    // Optionally store private key (less secure, but necessary if private key regeneration is not possible)
    unsigned char privateKey[1600];
    ret = mbedtls_pk_write_key_pem(&pk_ctx, privateKey, sizeof(privateKey));
    if (ret == 0) {
        preferences.putString("privateKey", (const char *)privateKey);
        Serial.println("Stored private key in NVS");
    } else {
        Serial.println("Failed to write private key to PEM format");
    }
}

bool loadKeyPair() {
    String publicKey = preferences.getString("publicKey", "");
    if (publicKey == "") {
        Serial.println("No public key found in NVS");
        return false;
    }

    int ret = mbedtls_pk_parse_public_key(&pk_ctx, (const unsigned char *)publicKey.c_str(), publicKey.length() + 1);
    if (ret != 0) {
        Serial.println("Failed to parse public key from NVS");
        return false;
    }

    String privateKey = preferences.getString("privateKey", "");
    if (privateKey != "") {
        ret = mbedtls_pk_parse_key(&pk_ctx, (const unsigned char *)privateKey.c_str(), privateKey.length() + 1, NULL, 0);
        if (ret != 0) {
            Serial.println("Failed to parse private key from NVS");
            return false;
        }
    }

    Serial.println("Loaded key pair from NVS");
    return true;
}

void saveWiFiCredentials(const String &ssid, const String &password) {
    wifiSSID = ssid;
    wifiPassword = password;
    preferences.putString("wifiSSID", wifiSSID);
    preferences.putString("wifiPassword", wifiPassword);
    Serial.println("WiFi credentials updated and stored in NVS");
}

void saveServerAddress(const String &address) {
    serverAddress = address;
    preferences.putString("serverAddress", serverAddress);
    Serial.println("Server address updated and stored in NVS");
}

void savePresetPassword(const String &password) {
    presetPassword = password;
    preferences.putString("presetPassword", presetPassword);
    Serial.println("Preset password updated and stored in NVS");
}

void loadStoredData() {
    // Load stored data or use default/preset values if not found
    wifiSSID = preferences.getString("wifiSSID", INITIAL_WIFI_SSID);
    wifiPassword = preferences.getString("wifiPassword", INITIAL_WIFI_PASSWORD);
    serverAddress = preferences.getString("serverAddress", INITIAL_SERVER_ADDRESS);
    presetPassword = preferences.getString("presetPassword", INITIAL_PRESET_PASSWORD);
    Serial.println("Loaded stored data from NVS or used preset values if not found");
}

void sendDoorIDAndPublicKey() {
    // Ensure the public key is available
    unsigned char publicKey[1600];
    int ret = mbedtls_pk_write_pubkey_pem(&pk_ctx, publicKey, sizeof(publicKey));
    if (ret != 0) {
        Serial.println("Failed to write public key to PEM format");
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Use serverAddress if available, otherwise use INITIAL_SERVER_ADDRESS
        String url = serverAddress.isEmpty() ? String(INITIAL_SERVER_ADDRESS) : serverAddress;
        url += "/api/register"; // API endpoint

        http.begin(url); // OpenLock server API endpoint
        http.addHeader("Content-Type", "application/json");

        // Create JSON document with Door ID and Public Key
        DynamicJsonDocument jsonDoc(1024);
        jsonDoc["doorID"] = DOOR_ID;
        jsonDoc["publicKey"] = (const char *)publicKey;

        String requestBody;
        serializeJson(jsonDoc, requestBody);

        // Send POST request with the JSON body
        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {
            Serial.printf("Response code: %d\n", httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        } else {
            Serial.printf("Error in sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    } else {
        Serial.println("WiFi not connected, unable to send Door ID and Public Key.");
    }
}

void sendPublicKey() {
    unsigned char publicKey[1600];
    int ret = mbedtls_pk_write_pubkey_pem(&pk_ctx, publicKey, sizeof(publicKey));
    if (ret == 0) {
        String publicKeyStr = (const char *)publicKey;
        webSocketClient.send(publicKeyStr.c_str());
        Serial.println("Sent public key to server");
    } else {
        Serial.println("Failed to write public key to PEM format");
    }
}

void sendStatus() {
    String status = "connected";
    if (WiFi.status() != WL_CONNECTED) {
        status = "disconnected";
    }
    String statusMessage = String("{\"status\":\"") + status + "\"}";
    webSocketClient.send(statusMessage.c_str());
    Serial.println("Sent status to server");
}
