#include "HassioMqttHandler.h"

HassioMqttConnectionManager::HassioMqttConnectionManager(double hardwareVersion, double softwareVersion, const char * author, const char * deviceId, const char * deviceName) {
    this->lastMqttUpdateTime = 0;
    this->wifiClient = WiFiClient();
    pubSubClient = PubSubClient(wifiClient);
    this->hardwareVersion = hardwareVersion;
    this->softwareVersion = softwareVersion;
    this->author = author;
    this->deviceId = deviceId;
    this->deviceName = deviceName;
}

DynamicJsonDocument HassioMqttConnectionManager::getDeviceInfoJson() {
    DynamicJsonDocument deviceInfo(DEFAULT_JSON_SIZE);
    deviceInfo["hw_version"] = hardwareVersion;
    deviceInfo["sw_version"] = softwareVersion;
    deviceInfo["manufacturer"] = author;
    deviceInfo["identifiers"] = deviceId;
    deviceInfo["name"] = deviceName;
    return deviceInfo;
}

DynamicJsonDocument HassioMqttConnectionManager::buildDiscoveryStub(String name, String id) {
    DynamicJsonDocument doc(DEFAULT_JSON_SIZE);
    doc["name"] = name;
    doc["uniq_id"] = id;
    doc["stat_t"] = stateTopic;
    doc["device"] = getDeviceInfoJson();
    return doc;
}

void HassioMqttConnectionManager::setup(const char * host, int port, const char * user, const char * pass, MQTT_CALLBACK_SIGNATURE) {
    pubSubClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    pubSubClient.setServer(host, port);
    pubSubClient.setCallback(callback);
    while (!pubSubClient.connected()) {
        Serial.print(".");

        if (pubSubClient.connect(mqttName.c_str(), user, pass)) {
        Serial.println("Connected to MQTT");
        sendMQTTAmountDiscoveryMessage();
        sendMQTTWeightDiscoveryMessage();
        sendMQTTRunningDiscoveryMessage();
        sendMQTTWeightBasedDiscoveryMessage();
        sendMQTTCloggedDiscoveryMessage();
        sendMQTTFlowDiscoveryMessage();
        sendMQTTScaleZeroDiscoveryMessage();
        sendMQTTClogToleranceDiscoveryMessage();
        sendMQTTPullbackDegreesDiscoveryMessage();
        sendMQTTLastDosisDiscoveryMessage();
        sendMQTTSpeedDiscoveryMessage();
        pubSubClient.subscribe(dosageCmdTopic.c_str());
        pubSubClient.subscribe(runningCmdTopic.c_str());
        pubSubClient.subscribe(weightBasedCmdTopic.c_str());
        pubSubClient.subscribe(flowCmdTopic.c_str());
        pubSubClient.subscribe(scaleZeroCmdTopic.c_str());
        pubSubClient.subscribe(clogToleranceCmdTopic.c_str());
        pubSubClient.subscribe(pullbackDegreesCmdTopic.c_str());
        pubSubClient.subscribe(speedCmdTopic.c_str());
        } else {
        Serial.println("failed with state ");
        Serial.print(pubSubClient.state());
        delay(2000);
        }
    }
}

void HassioMqttConnectionManager::loop() {
    pubSubClient.loop();
}

void HassioMqttConnectionManager::HassioMqttConnectionManager::publishStatus(
    int weight, int amount, bool isRunning, bool isWeightBased, bool isClogged, int flow, 
    int scaleZero, int clogTolerance, int pullbackDegrees, int lastDosis, int speed
) {
    DynamicJsonDocument doc(DEFAULT_JSON_SIZE);
    char buffer[256];

    doc["weight"] = weight;
    doc["dosage"] = amount;
    doc["running"] = isRunning;
    doc["weight_based"] = isWeightBased;
    doc["clogged"] = isClogged;
    doc["flow"] = flow;
    doc["scale_zero"] = scaleZero;
    doc["clog_tolerance"] = clogTolerance;
    doc["pullback_degrees"] = pullbackDegrees;
    doc["last_dosis"] = lastDosis;
    doc["speed"] = speed;

    size_t n = serializeJson(doc, buffer);
    Serial.print("Sending mqtt status: ");
    String message;
    serializeJson(doc, message);
    Serial.println(message);

    pubSubClient.publish(stateTopic.c_str(), buffer, n);
    this->lastMqttUpdateTime = millis();
}

void HassioMqttConnectionManager::sendMQTTDiscoveryMessage(String discoveryTopic, DynamicJsonDocument doc) {
    char buffer[MQTT_MAX_PACKET_SIZE];
    size_t n = serializeJson(doc, buffer);
    String message;
    for (unsigned int i = 0; i < n; i++) {
        message = message + buffer[i];  // convert *byte to string
    }
    Serial.print("Sending discovery: ");
    Serial.println(message);
    Serial.println(pubSubClient.publish(discoveryTopic.c_str(), buffer, n));
}

void HassioMqttConnectionManager::sendMQTTWeightDiscoveryMessage() {
    String discoveryTopic = "homeassistant/sensor/cat_feeder/weight/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF remaining food", "cf_weight");
    doc["icon"] = "mdi:food-drumstick";
    doc["unit_of_meas"] = "g";
    doc["frc_upd"] = false;
    doc["val_tpl"] = "{{ value_json.weight|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTAmountDiscoveryMessage() {
    String discoveryTopic = "homeassistant/number/cat_feeder/amount/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF dosage", "cf_amount");
    doc["icon"] = "mdi:weight-gram";
    doc["cmd_t"] = dosageCmdTopic;
    doc["min"] = 0;
    doc["max"] = 500;
    doc["unit_of_meas"] = "g";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.dosage|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTRunningDiscoveryMessage() {
    String discoveryTopic = "homeassistant/switch/cat_feeder/running/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF running", "cf_running");
    doc["icon"] = "mdi:food";
    doc["cmd_t"] = runningCmdTopic;
    doc["payload_on"] = true;
    doc["payload_off"] = false;
    doc["val_tpl"] = "{{ value_json.running|default(false) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTWeightBasedDiscoveryMessage() {
    String discoveryTopic = "homeassistant/switch/cat_feeder/weight_based/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Weight Based", "cf_weight_based");
    doc["icon"] = "mdi:weight";
    doc["cmd_t"] = weightBasedCmdTopic;
    doc["payload_on"] = true;
    doc["payload_off"] = false;
    doc["val_tpl"] = "{{ value_json.weight_based|default(false) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTCloggedDiscoveryMessage() {
    String discoveryTopic = "homeassistant/binary_sensor/cat_feeder/clogged/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Clogged", "cf_clogged");
    doc["dev_cla"] = "problem";
    doc["payload_on"] = true;
    doc["payload_off"] = false;
    doc["val_tpl"] = "{{ value_json.clogged|default(false) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTFlowDiscoveryMessage() {
    String discoveryTopic = "homeassistant/number/cat_feeder/flow/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Revolution flow", "cf_flow");
    doc["icon"] = "mdi:fan-auto";
    doc["cmd_t"] = flowCmdTopic;
    doc["min"] = 0;
    doc["max"] = 100;
    doc["unit_of_meas"] = "g";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.flow|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTScaleZeroDiscoveryMessage() {
    String discoveryTopic = "homeassistant/number/cat_feeder/scale_zero/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Scale Zero", "cf_scale_zero");
    doc["icon"] = "mdi:fan-auto";
    doc["cmd_t"] = scaleZeroCmdTopic;
    doc["min"] = -1000;
    doc["max"] = 1000;
    doc["unit_of_meas"] = "g";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.scale_zero|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTClogToleranceDiscoveryMessage() {
    String discoveryTopic = "homeassistant/number/cat_feeder/clog_tolerance/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Clog Tolerance", "cf_clog_tolerance");
    doc["icon"] = "mdi:cookie-refresh-outline";
    doc["cmd_t"] = clogToleranceCmdTopic;
    doc["min"] = 0;
    doc["max"] = 100;
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.clog_tolerance|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTPullbackDegreesDiscoveryMessage() {
    String discoveryTopic = "homeassistant/number/cat_feeder/pullback_degrees/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Pullback Degrees", "cf_pullback_degrees");
    doc["icon"] = "mdi:skip-backward-outline";
    doc["cmd_t"] = pullbackDegreesCmdTopic;
    doc["min"] = 0;
    doc["max"] = 360;
    doc["unit_of_meas"] = "º";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.pullback_degrees|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTLastDosisDiscoveryMessage() {
    String discoveryTopic = "homeassistant/sensor/cat_feeder/last_dosis/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Last Dosis", "cf_last_dosis");
    doc["icon"] = "mdi:food-drumstick-outline";
    doc["unit_of_meas"] = "g";
    doc["frc_upd"] = false;
    doc["val_tpl"] = "{{ value_json.last_dosis|default(0) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void HassioMqttConnectionManager::sendMQTTSpeedDiscoveryMessage() {
    String discoveryTopic = "homeassistant/number/cat_feeder/speed/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Speed", "cf_speed");
    doc["icon"] = "mdi:speedometer";
    doc["cmd_t"] = speedCmdTopic;
    doc["min"] = 0;
    doc["max"] = 100;
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.speed|default(10) }}";
    sendMQTTDiscoveryMessage(discoveryTopic, doc);
}
