#ifndef HassioMqttHandler_h
#define HassioMqttHandler_h

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

// MQTT Constants
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_PERIODIC_UPDATE_INTERVAL 10000
#define DEFAULT_JSON_SIZE 1024

class HassioMqttConnectionManager {
    private:
        WiFiClient wifiClient;
        PubSubClient pubSubClient;
        double hardwareVersion;
        double softwareVersion;
        const char * author;
        const char * deviceId;
        const char * deviceName;

        DynamicJsonDocument getDeviceInfoJson();
        DynamicJsonDocument buildDiscoveryStub(String name, String id);
        void sendMQTTDiscoveryMessage(String discoveryTopic, DynamicJsonDocument doc);
        void sendMQTTWeightDiscoveryMessage();
        void sendMQTTAmountDiscoveryMessage();
        void sendMQTTRunningDiscoveryMessage();
        void sendMQTTWeightBasedDiscoveryMessage();
        void sendMQTTCloggedDiscoveryMessage();
        void sendMQTTFlowDiscoveryMessage();
        void sendMQTTScaleZeroDiscoveryMessage();
        void sendMQTTClogToleranceDiscoveryMessage();
        void sendMQTTPullbackDegreesDiscoveryMessage();
        void sendMQTTLastDosisDiscoveryMessage();
        void sendMQTTSpeedDiscoveryMessage();
    public:
        unsigned long lastMqttUpdateTime;
        // MQTT Config
        const String mqttName = "Cat Feeder";
        const String stateTopic = "home/cat_feeder/state";
        const String dosageCmdTopic = "home/cat_feeder/dosage";
        const String runningCmdTopic = "home/cat_feeder/running";
        const String weightBasedCmdTopic = "home/cat_feeder/weight_based";
        const String flowCmdTopic = "home/cat_feeder/flow";
        const String scaleZeroCmdTopic = "home/cat_feeder/scale_zero";
        const String clogToleranceCmdTopic = "home/cat_feeder/clog_tolerance";
        const String pullbackDegreesCmdTopic = "home/cat_feeder/pullback_degrees";
        const String speedCmdTopic = "home/cat_feeder/speed";

        HassioMqttConnectionManager(double hardwareVersion, double softwareVersion, const char * author, const char * deviceId, const char * deviceName);
        void publishStatus(float weight, int amount, bool isRunning, bool isWeightBased, bool isClogged, int flow, int scaleZero, int clogTolerance, int pullbackDegrees, int lastDosis, int speed);
        void setup(const char * host, int port, const char * user, const char * pass, MQTT_CALLBACK_SIGNATURE);
        void loop();
};
#endif
