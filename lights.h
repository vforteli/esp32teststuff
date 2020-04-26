#ifndef LIGHTS_H
#define LIGHTS_H

#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

int setLights(bool on)
{
    HTTPClient http;
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

#endif
