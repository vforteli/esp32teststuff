#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Esp32MQTTClient.h>
#include <ArduinoJson.h>
#include "config.h"

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int lightSensorPin = 39;
const int pirSensorPin = 25;

static bool hasIoTHub = false;
int x = 0;
int previous_y = 0;
int previous_x = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup()
{
  Serial.begin(115200);

  pinMode(pirSensorPin, INPUT);

  // Start I2C Communication SDA = 5 and SCL = 4 on Wemos Lolin32 ESP32 with built-in SSD1306 OLED
  Wire.begin(5, 4);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false))
  {
    Serial.println(F("SSD1306 allocation failed"));
  }

  display.clearDisplay();
  displayText("Starting up");
  delay(500);

  WiFi.begin(ssid, key);

  while (WiFi.status() != WL_CONNECTED)
  {
    displayText("Connecting to wifi...");
    delay(500);
  }

  displayText("Wifi connected \\o/");
  delay(500);

  if (!Esp32MQTTClient_Init((const uint8_t *)connectionString))
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return;
  }
  hasIoTHub = true;

  previous_y = normalizeToGraph(analogRead(lightSensorPin));
}

const String lightLevelText = "Light level: ";

void loop()
{
  float lightValue = analogRead(lightSensorPin);

  displayText(lightLevelText + lightValue);

  int y = normalizeToGraph(lightValue);

  display.drawLine(previous_x, previous_y, x, y, WHITE);
  display.display();
  previous_y = y;
  previous_x = x;
  x += 1;
  if (x > 127)
  {
    display.clearDisplay();
    x = 0;
    previous_x = 0;
  }

  bool movementDetected = digitalRead(pirSensorPin);

  if (movementDetected)
  {
    Serial.println("Presence detected");
  }
  else
  {
    Serial.println("no movement?!");
  }

  if (hasIoTHub)
  {
    DynamicJsonDocument json(128);
    json["lightLevel"] = lightValue;
    json["movementDetected"] = movementDetected;

    char buffer[128];
    serializeJson(json, buffer);

    if (Esp32MQTTClient_SendEvent(buffer))
    {
      Serial.println("Sending data succeed");
    }
    else
    {
      Serial.println("Failure...");
    }
  }

  delay(1000); // todo convert to non blocking
}

int normalizeToGraph(float value)
{
  return (value / 4095) * (20 - 63) + 63;
}

void displayText(String value)
{
  Serial.println(value);

  for (int y = 0; y < 8; y++)
  {
    for (int x = 0; x < SCREEN_WIDTH; x++)
    {
      display.drawPixel(x, y, BLACK);
    }
  }
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(value);
  display.display();
}
