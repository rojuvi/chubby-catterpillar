#include "HassioMqttPayload.h"

HassioMqttPayload::HassioMqttPayload(const char* topic, DynamicJsonDocument message) {
    this->topic = topic;
    this->messageSize = serializeJson(message, this->buffer);

}