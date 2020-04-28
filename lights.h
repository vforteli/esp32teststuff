#ifndef LIGHTS_H
#define LIGHTS_H

#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

HTTPClient http;

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

int getLight(int id)
{
    http.begin(hueApiUrl + "lights/" + id);
    DynamicJsonDocument document(1000);
    int resultCode = http.GET();
    deserializeJson(document, http.getStream());
    http.end();
    String foo = "Light: ";
    String name = document["name"];
    bool on = document["state"]["on"];
    Serial.println(foo + name + " status: " + on);
}

int getGroupLights(int groupId)
{
    http.begin(hueApiUrl + "groups/" + groupId);
    DynamicJsonDocument document(1000);
    int resultCode = http.GET();
    deserializeJson(document, http.getStream());
    http.end();
    String name = document["name"];
    Serial.println(name);

    JsonArray array = document["lights"].as<JsonArray>();
    for (JsonVariant v : array)
    {
        int lightId = v.as<int>();
        Serial.println(lightId);
        getLight(lightId);
    }
}

#endif
