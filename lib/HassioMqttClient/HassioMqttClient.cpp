#include "HassioMqttClient.h"

HassioMqttClient::HassioMqttClient(HassioMqttContractBuilder& contractBuilder) {
    this->lastMqttUpdateTime = 0;
    this->wifiClient = WiFiClient();
    pubSubClient = PubSubClient(wifiClient);
    this->contractBuilder = contractBuilder;
}

void HassioMqttClient::setup(const char * host, int port, const char * user, const char * pass, MQTT_CALLBACK_SIGNATURE) {
    pubSubClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    pubSubClient.setServer(host, port);
    pubSubClient.setCallback(callback);
    while (!pubSubClient.connected()) {
        Serial.print(".");

        if (pubSubClient.connect(this->contractBuilder.mqttName, user, pass)) {
            Serial.println("Connected to MQTT");
            publishMessage(this->contractBuilder.getMQTTAmountDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTWeightDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTRunningDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTWeightBasedDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTCloggedDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTFlowDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTScaleZeroDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTClogToleranceDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTPullbackDegreesDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTLastDosisDiscoveryPayload());
            publishMessage(this->contractBuilder.getMQTTSpeedDiscoveryPayload());
            pubSubClient.subscribe(this->contractBuilder.dosageCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.runningCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.weightBasedCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.flowCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.scaleZeroCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.clogToleranceCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.pullbackDegreesCmdTopic);
            pubSubClient.subscribe(this->contractBuilder.speedCmdTopic);
        } else {
            Serial.println("failed with state ");
            Serial.print(pubSubClient.state());
            delay(2000);
        }
    }
}

void HassioMqttClient::loop() {
    pubSubClient.loop();
}

void HassioMqttClient::publishMessage(HassioMqttPayload* payload) {
    pubSubClient.publish(payload->topic, payload->buffer, payload->messageSize);
    this->lastMqttUpdateTime = millis();
}