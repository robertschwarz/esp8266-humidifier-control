
/*
This code is mainly cobbled together from different tutorials that I found online.

These are the tutorials that I used:

SSD1306 + DHT Temparature/Humidity Setup: 
https://makersportal.com/blog/2018/9/3/arduino-i2c-oled-display-temperature-and-humidity

ESP8266 WiFi Setup:
https://tttapa.github.io/ESP8266/Chap07%20-%20Wi-Fi%20Connections.html

Tutorial on how to use Asynjson, ArduinoJson and ESPAsyncWebserver:
https://raphaelpralat.medium.com/example-of-json-rest-api-for-esp32-4a5f64774a05

Tutorial on how to build a REST API with Arduino:
https://www.mischianti.org/2020/05/16/how-to-create-a-rest-server-on-esp8266-and-esp32-startup-part-1/
*/

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
const char *philips_hue_url = "YOUR_PHILIPS_HUE_URL";

// Import required libraries
#include <FS.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <DHT_U.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include <ESP8266HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define DHTTYPE DHT11
#define DHTPIN 14

WiFiClient wifiClient;
DHT dht(DHTPIN, DHTTYPE);
float humi;
float tempC;
int autoMode;
int minHumidity;
int maxHumidity;
int humidifier;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void setup()
{
  autoMode = 0;

  Serial.begin(115200);

  dht.begin();

  /* DISPLAY SETUP */

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Use address is 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  if (!LittleFS.begin())
  {
    Serial.println("An error occured launching the filesystem.");
    return;
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  /* Part 1 of the server: 
    Load the HTML Server content
  */
  server.on("/home", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html"); });

  server.on("/pico.classless.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/pico.classless.min.css", "text/css"); });

  server.on("/reef.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/reef.min.js", "text/javascript"); });

  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/app.js", "text/javascript"); });

  /* Part 2 of the server:
    GET API Endpoint to fetch temparature information from the arduino's DHT11
  */
  server.on("/api/metadata", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              DynamicJsonDocument json(1024);
              json["humidity"] = humi;
              json["temperature"] = tempC;
              json["autoMode"] = autoMode;
              json["minHumidity"] = minHumidity;
              json["maxHumidity"] = maxHumidity;
              json["humidifier"] = humidifier;

              serializeJson(json, *response);
              request->send(response);
            });

  /* Part 3 of the server:
    POST API Endpoint to send data from the frontend to the arduino
  */
  server.on("/api/auto", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("auto")) {
                autoMode = request->getParam("auto")->value().toInt();
                Serial.println("Automatic Mode toggled: ");
                Serial.print(autoMode);
              }
              
              request->send(200, "text/plain", "Toggled Automode!");
            });

  server.on("/api/status", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("on")) {
                humidifier = request->getParam("on")->value().toInt();
                Serial.println("Humidifier Status toggled: ");
                Serial.print(humidifier);
              }
              
              request->send(200, "text/plain", "Saved Humidifier Status!");
            });

  server.on("/api/range", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("min") && request->hasParam("max"))
              {
                minHumidity = request->getParam("min")->value().toInt();
                maxHumidity = request->getParam("max")->value().toInt();
                
                Serial.println("Set min Humidity: ");
                Serial.print(minHumidity);
                Serial.println("Set max Humidity: ");
                Serial.print(maxHumidity);
              }
              request->send(200, "text/plain", "Range set!");
            });

  // Start server
  server.begin();
}


void oledDisplayHeader() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Humidity");
  if (autoMode == 1) {
    display.setCursor(0, 10);
    display.print("Auto Mode ON!");
  }

  if (autoMode == 0) {
    display.setCursor(0, 10);
    display.print("Auto Mode OFF!");
  }
  display.setCursor(60, 0);
  display.print("Temperature");
}

void oledDisplay(int size, int x, int y, float value, String unit) {
  int charLen = 12;
  int xo = x + charLen * 4;
  int xunit = x + charLen * 4.4;
  int xval = x;
  display.setTextSize(size);
  display.setTextColor(WHITE);

  if (unit == "%") {
    display.setCursor(x, y);
    display.print(value, 0);
    display.print(unit);
  } else {
    if (value > 99) {
      xval = x;
    } else {
      xval = x + charLen;
    }
    display.setCursor(xval, y);
    display.print(value, 0);
    display.drawCircle(xo, y + 2, 2, WHITE);
    display.setCursor(xunit, y);
    display.print(unit);
  }
}

void loop()
{
  humi = dht.readHumidity();
  tempC = dht.readTemperature();

  HTTPClient http;
  http.begin(wifiClient, philips_hue_url);
  http.addHeader("Content-Type", "application/json");

  display.clearDisplay();
  oledDisplayHeader();
  oledDisplay(3, 5, 28, humi, "%");
  oledDisplay(3, 55, 28, tempC, "C");
  display.display();

  if (autoMode == 1)
  {
    if (humi != 0 && humi <= minHumidity && humidifier == 0)
    {
      humidifier = 1;
      http.PUT("{\"on\":true}");
      Serial.println("Turned humidifier on.");
    }

    if (humi != 0 && humi >= maxHumidity && humidifier == 1)
    {
      humidifier = 0;
      http.PUT("{\"on\":false}");
      Serial.println("Turned humidifier off.");
    }
  }
  http.end();
  delay(10000);
}