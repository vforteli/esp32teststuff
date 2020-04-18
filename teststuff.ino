#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int sensorPin = 26;
int x = 0;
int previous_y;
int previous_x = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup()
{
  Serial.begin(115200);

  // Start I2C Communication SDA = 5 and SCL = 4 on Wemos Lolin32 ESP32 with built-in SSD1306 OLED
  Wire.begin(5, 4);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false))
  {
    Serial.println(F("SSD1306 allocation failed"));
  }

  display.clearDisplay();
  displayText("Starting up");
  delay(500);

  float lightValue = analogRead(sensorPin);
  previous_y = (lightValue / 4095) * (20 - 63) + 63;
}

const String lightLevelText = "Light level: ";

void loop()
{
  float lightValue = analogRead(sensorPin);

  Serial.println(lightLevelText + lightValue);
  displayText(lightLevelText + lightValue);

  int y = (lightValue / 4095) * (20 - 63) + 63;

  Serial.println(y);
  //display.drawPixel(x, y, WHITE);
  display.drawLine(previous_x, previous_y, x, y, WHITE);
  previous_y = y;
  previous_x = x;
  x += 1;
  if (x > 127)
  {
    display.clearDisplay();
    x = 0;
    previous_x = 0;
  }

  delay(500);
}

void displayText(String value)
{
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
