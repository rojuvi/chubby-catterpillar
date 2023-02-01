#ifndef HassioMqttHandler_h
#define HassioMqttHandler_h

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <HassioMqttPayload.h>
#include <HassioMqttContractBuilder.h>

// MQTT Constants
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_PERIODIC_UPDATE_INTERVAL 1000

class HassioMqttClient {
    private:
        WiFiClient wifiClient;
        PubSubClient pubSubClient;
        HassioMqttContractBuilder contractBuilder;
        double hardwareVersion;
        double softwareVersion;
        const char * author;
        const char * deviceId;
        const char * deviceName;
    public:
        unsigned long lastMqttUpdateTime;

        HassioMqttClient(HassioMqttContractBuilder& contractBuilder);
        void setup(const char * host, int port, const char * user, const char * pass, MQTT_CALLBACK_SIGNATURE);
        void loop();
        void publishMessage(HassioMqttPayload payload);
};
#endif