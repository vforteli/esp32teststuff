#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Esp32MQTTClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "config.h"
#include "utils.h"

const int SDA_PIN = 5;
const int SCL_PIN = 4;
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int graphMinY = 10;
const int graphMaxY = 57;

const int LIGHT_SENSOR_PIN = 39;
const int PIR_SENSOR_PIN = 25;

static bool hasIoTHub = false;
int x = 0;
int previous_y = 0;
int previous_x = 0;
int dimmerGracePeriod = 10 * 1000;
long lastMovementDetected = 0;
bool lightsTouched = false;
bool lightsOn = true;          // makes more sense to assume lights are on. logic should not  turn on lights ever if initial state is on. in the future, call hue api to get initial state
bool autoLightsEnabled = true; // maybe toggle from touch pin

HTTPClient http;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup()
{
  Serial.begin(115200);

  lastMovementDetected = millis();

  pinMode(PIR_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);

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

  previous_y = normalizeToGraph(analogRead(LIGHT_SENSOR_PIN));
}

const String lightLevelText = "Light level: ";

void loop()
{
  long loopStart = millis();

  float lightValue = analogRead(LIGHT_SENSOR_PIN);
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

  bool movementDetected = digitalRead(PIR_SENSOR_PIN);
  Serial.printf("Movement detected: %d\n", movementDetected);

  long now = millis();

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
    int foo = scale((float)(now - lastMovementDetected), 0, dimmerGracePeriod, 0, SCREEN_WIDTH);
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
    long startMillis = millis();
    DynamicJsonDocument json(128);
    json["lightLevel"] = lightValue;
    json["movementDetected"] = movementDetected;
    json["lightsOn"] = lightsOn;

    char buffer[128];
    serializeJson(json, buffer);

    if (Esp32MQTTClient_SendEvent(buffer))
    {
      Serial.println("Sending data succeeded");
    }
    else
    {
      Serial.println("Failure...");
    }

    Serial.printf("Sending message took %d ms\n", millis() - startMillis);
  }

  Serial.printf("Loop took %d ms\n", millis() - loopStart);
  delay(1000); // todo convert to non blocking
}

int normalizeToGraph(float value)
{
  return scale(value, 0, 4095, graphMaxY, graphMinY);
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
  //json["transitiontime"] = 200; // 20 seconds

  char buffer[128];
  serializeJson(json, buffer);
  int result = http.PUT(buffer);
  http.end();
  Serial.printf("Http result: %d\n", result);

  return result;
}

void drawUiGrid()
{
  display.drawLine(0, 9, SCREEN_WIDTH, 9, WHITE);
  display.drawLine(0, 58, SCREEN_WIDTH, 58, WHITE);
}
