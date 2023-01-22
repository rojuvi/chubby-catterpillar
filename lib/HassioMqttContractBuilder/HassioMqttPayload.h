#ifndef HassioMqttPayload_h
#define HassioMqttPayload_h

#include <ArduinoJson.h>

#define DEFAULT_JSON_SIZE 1024

class HassioMqttPayload {
    public:
        const char* topic;
        size_t messageSize;
        char buffer[DEFAULT_JSON_SIZE];
        HassioMqttPayload(const char* topic, DynamicJsonDocument message);
};

#endif