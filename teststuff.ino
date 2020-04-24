#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Esp32MQTTClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "config.h"

const int SDA_PIN = 5;
const int SCL_PIN = 4;
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int lightSensorPin = 39;
const int pirSensorPin = 25;

static bool hasIoTHub = false;
int x = 0;
int previous_y = 0;
int previous_x = 0;
int dimmerGracePeriod = 10 * 1000;
long lastMovementDetected = 0;
bool lightsTouched = false;
bool lightsOn = true;           // makes more sense to assume lights are on. logic should not  turn on lights ever if initial state is on. in the future, call hue api to get initial state
bool autoLightsEnabled = false; // maybe toggle from touch pin

HTTPClient http;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup()
{
  Serial.begin(115200);

  lastMovementDetected = millis();

  pinMode(pirSensorPin, INPUT);
  pinMode(lightSensorPin, INPUT);

  setupDisplay();
  delay(500);

  connectWifi();
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

  displayText(lightLevelText + (uint)lightValue);

  int y = normalizeToGraph(lightValue);

  display.drawLine(previous_x, previous_y, x, y, WHITE);
  display.display();
  previous_y = y;
  previous_x = x;
  x += 1;
  if (x > 127)
  {
    display.fillRect(0, 10, SCREEN_WIDTH, 47, BLACK);
    x = 0;
    previous_x = 0;
  }

  bool movementDetected = digitalRead(pirSensorPin);
  Serial.print("Movement detected: ");
  Serial.println(movementDetected);

  long now = millis();
  lastMovementDetected = now;

  if (movementDetected)
  {
    lastMovementDetected = now;
    display.fillRect(0, 59, SCREEN_WIDTH, 5, BLACK);
    display.display();

    if (autoLightsEnabled && lightsTouched && !lightsOn)
    {
      int result = setLights(true);
      if (result == HTTP_CODE_OK)
      {
        Serial.println("Lights turned on");
        lightsOn = true;
        lightsTouched = true;
      }
    }
  }
  else
  {
    int foo = normalize((float)(now - lastMovementDetected), 0, dimmerGracePeriod, 0, SCREEN_WIDTH);
    Serial.print("x should be: ");
    Serial.println(foo);
    display.fillRect(0, 59, foo, 5, WHITE);
    display.display();

    if (autoLightsEnabled && lastMovementDetected + dimmerGracePeriod < now && lightsOn)
    {
      int result = setLights(false);
      if (result == HTTP_CODE_OK)
      {
        Serial.println("Lights turned off");
        lightsOn = false;
        lightsTouched = true;
      }
    }
  }

  if (hasIoTHub)
  {
    DynamicJsonDocument json(128);
    json["lightLevel"] = lightValue;
    json["movementDetected"] = movementDetected;
    json["lightsOn"] = lightsOn;

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
  int max_output = 57;
  return (value / 4095) * (20 - max_output) + max_output;
}

// todo fix and refactor, yes yes they are reversed...
int normalize(float value, int min_input, int max_input, int max_output, int min_output)
{
  return (value / max_input) * (min_output - max_output) + max_output;
}

void displayText(String value)
{
  Serial.println(value);

  display.fillRect(0, 0, SCREEN_WIDTH, 8, BLACK);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(value);
  display.display();
}

void setupDisplay()
{
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false))
  {
    Serial.println(F("SSD1306 allocation failed"));
  }

  display.clearDisplay();
  drawUiGrid();
  display.display();

  displayText("Starting up");
}

void connectWifi()
{
  WiFi.begin(ssid, key);

  while (WiFi.status() != WL_CONNECTED)
  {
    displayText("Connecting to wifi...");
    delay(500);
  }

  displayText("Wifi connected \\o/");
}

int setLights(bool on)
{
  Serial.print("Settings lights to: ");
  Serial.println(on);
  http.begin(hueApiUrl + "groups/5/action");
  DynamicJsonDocument json(128);
  json["on"] = on;

  char buffer[128];
  serializeJson(json, buffer);
  int result = http.PUT(buffer);
  Serial.print("Http result: ");
  Serial.println(result);
  http.end();

  return result;
}

void drawUiGrid()
{
  display.drawLine(0, 9, SCREEN_WIDTH, 9, WHITE);
  display.drawLine(0, 58, SCREEN_WIDTH, 58, WHITE);
}
