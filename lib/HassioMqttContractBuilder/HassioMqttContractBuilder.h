#ifndef HassioMqttContractBuilder_h
#define HassioMqttContractBuilder_h

#include <ArduinoJson.h>
#include "HassioMqttPayload.h"

class HassioMqttContractBuilder {
    private:
        double hardwareVersion;
        double softwareVersion;
        const char * author;
        const char * deviceId;
        const char * deviceName;

        DynamicJsonDocument getDeviceInfoJson();
        DynamicJsonDocument buildDiscoveryStub(const char* name, const char* id);
    public:
        // MQTT Config
        const char* mqttName = "Cat Feeder";
        const char* stateTopic = "home/cat_feeder/state";
        const char* dosageCmdTopic = "home/cat_feeder/dosage";
        const char* runningCmdTopic = "home/cat_feeder/running";
        const char* weightBasedCmdTopic = "home/cat_feeder/weight_based";
        const char* flowCmdTopic = "home/cat_feeder/flow";
        const char* scaleZeroCmdTopic = "home/cat_feeder/scale_zero";
        const char* clogToleranceCmdTopic = "home/cat_feeder/clog_tolerance";
        const char* pullbackDegreesCmdTopic = "home/cat_feeder/pullback_degrees";
        const char* speedCmdTopic = "home/cat_feeder/speed";

        HassioMqttContractBuilder();
        HassioMqttContractBuilder(double hardwareVersion, double softwareVersion, const char * author, const char * deviceId, const char * deviceName);
        HassioMqttPayload getMQTTWeightDiscoveryPayload();
        HassioMqttPayload getMQTTAmountDiscoveryPayload();
        HassioMqttPayload getMQTTRunningDiscoveryPayload();
        HassioMqttPayload getMQTTWeightBasedDiscoveryPayload();
        HassioMqttPayload getMQTTCloggedDiscoveryPayload();
        HassioMqttPayload getMQTTFlowDiscoveryPayload();
        HassioMqttPayload getMQTTScaleZeroDiscoveryPayload();
        HassioMqttPayload getMQTTClogToleranceDiscoveryPayload();
        HassioMqttPayload getMQTTPullbackDegreesDiscoveryPayload();
        HassioMqttPayload getMQTTLastDosisDiscoveryPayload();
        HassioMqttPayload getMQTTSpeedDiscoveryPayload();
        HassioMqttPayload getStatusMessage(float weight, int amount, bool isRunning, bool isWeightBased, bool isClogged, int flow, int scaleZero, int clogTolerance, int pullbackDegrees, int lastDosis, int speed);
};
#endif
