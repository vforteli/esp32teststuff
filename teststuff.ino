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
const String lightLevelText = "Light level: ";

volatile bool hasIoTHub = false;

int x = 0;
int previous_y = 0;
int previous_x = 0;
int dimmerGracePeriod = 10 * 1000;
bool lightsTouched = false;
volatile bool lightsOn = true;     // makes more sense to assume lights are on. logic should not  turn on lights ever if initial state is on. in the future, call hue api to get initial state
bool autoLightsEnabled = false;    // maybe toggle from touch pin
volatile long movementStopped = 0; // use int to avoid need for mutex...
volatile uint lightLevel = 0;

HTTPClient http;
TaskHandle_t telemetryProcessorTask;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void IRAM_ATTR handleMovementChangedInterrupt()
{
  if (digitalRead(PIR_SENSOR_PIN) == LOW)
  {
    // maybe start a timer alarm interrupt here instead...
    movementStopped = millis();
  }
  else
  {
    // ...and cancel it here if applicable
    movementStopped = 0;
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIR_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(PIR_SENSOR_PIN), handleMovementChangedInterrupt, CHANGE);

  setupDisplay();
  delay(500);

  connectWifi();
  delay(500);

  Serial.println("Starting telemetry processor task");
  xTaskCreatePinnedToCore(startTelemetryProcessor, "telemetryProcessor", 10000, NULL, 0, &telemetryProcessorTask, 0);

  previous_y = normalizeToGraph(analogRead(LIGHT_SENSOR_PIN));
}

void loop()
{
  long loopStart = millis();

  uint lightValue = analogRead(LIGHT_SENSOR_PIN);
  lightLevel = lightValue;
  displayText(lightLevelText + lightValue);

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
  Serial.printf("Reading light resistor and drawing display took %d ms\n", millis() - loopStart);

  long now = millis();

  if (movementStopped != 0)
  {
    Serial.println("No movement detected...");
    int foo = scale((float)(now - movementStopped), 0, dimmerGracePeriod, 0, SCREEN_WIDTH);
    display.fillRect(0, 59, foo, 5, WHITE);
    display.display();

    if (autoLightsEnabled && movementStopped + dimmerGracePeriod < now && lightsOn)
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
  else
  {
    Serial.println("Movement detected!");
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

  Serial.printf("Loop took %d ms\n", millis() - loopStart);
  delay(100); // todo convert to non blocking
}

int normalizeToGraph(uint value)
{
  return scale(value, 0, 4095, graphMaxY, graphMinY);
}

void displayText(String value)
{
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

void startTelemetryProcessor(void *parameter)
{
  Serial.printf("Starting telemetry processor on core %d\n", xPortGetCoreID());

  if (!Esp32MQTTClient_Init((const uint8_t *)connectionString))
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return;
  }
  hasIoTHub = true;

  while (true)
  {
    if (hasIoTHub)
    {
      // todo mutex?
      long startMillis = millis();
      DynamicJsonDocument json(128);
      json["lightLevel"] = lightLevel;
      json["movementDetected"] = movementStopped == 0;
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
    delay(1000);
  }
}
