#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>


#define PIN       16  // Pin number for the NeoPixel
#define NUMPIXELS 3   // Number of NeoPixels in the strip

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

const char* ssid = "WIFI_SSID";
const char* password = "PASSWORD";

const char* serverName = "<OPENLOCK_API_ENDPOINT";

void setup() {
  // put your setup code here, to run once:
  strip.begin();
  strip.show();

  Serial.begin(115200);
  colorWipe(strip.Color(255, 0, 0), 50);  // Red
  delay(500);

  // Connecting to a WiFi Network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  colorWipe(strip.Color(0, 0, 255), 50);  // Bue
  delay(500);

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

    if (response == "open") {}
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}