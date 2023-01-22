#include "HassioMqttContractBuilder.h"

HassioMqttContractBuilder::HassioMqttContractBuilder(){}

HassioMqttContractBuilder::HassioMqttContractBuilder(double hardwareVersion, double softwareVersion, const char * author, const char * deviceId, const char * deviceName) {
    this->hardwareVersion = hardwareVersion;
    this->softwareVersion = softwareVersion;
    this->author = author;
    this->deviceId = deviceId;
    this->deviceName = deviceName;
}

DynamicJsonDocument HassioMqttContractBuilder::getDeviceInfoJson() {
    DynamicJsonDocument deviceInfo(DEFAULT_JSON_SIZE);
    deviceInfo["hw_version"] = hardwareVersion;
    deviceInfo["sw_version"] = softwareVersion;
    deviceInfo["manufacturer"] = author;
    deviceInfo["identifiers"] = deviceId;
    deviceInfo["name"] = deviceName;
    return deviceInfo;
}

DynamicJsonDocument HassioMqttContractBuilder::buildDiscoveryStub(const char* name, const char* id) {
    DynamicJsonDocument doc(DEFAULT_JSON_SIZE);
    doc["name"] = name;
    doc["uniq_id"] = id;
    doc["stat_t"] = stateTopic;
    doc["device"] = getDeviceInfoJson();
    return doc;
}

HassioMqttPayload HassioMqttContractBuilder::getStatusMessage(
    float weight, int amount, bool isRunning, bool isWeightBased, bool isClogged, int flow, 
    int scaleZero, int clogTolerance, int pullbackDegrees, int lastDosis, int speed
) {
    DynamicJsonDocument doc(DEFAULT_JSON_SIZE);

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

    return HassioMqttPayload(stateTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTWeightDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/sensor/cat_feeder/weight/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF remaining food", "cf_weight");
    doc["icon"] = "mdi:food-drumstick";
    doc["unit_of_meas"] = "g";
    doc["frc_upd"] = false;
    doc["val_tpl"] = "{{ value_json.weight|default(0.00) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTAmountDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/number/cat_feeder/amount/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF dosage", "cf_amount");
    doc["icon"] = "mdi:weight-gram";
    doc["cmd_t"] = dosageCmdTopic;
    doc["min"] = 0;
    doc["max"] = 500;
    doc["unit_of_meas"] = "g";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.dosage|default(0) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTRunningDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/switch/cat_feeder/running/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF running", "cf_running");
    doc["icon"] = "mdi:food";
    doc["cmd_t"] = runningCmdTopic;
    doc["payload_on"] = true;
    doc["payload_off"] = false;
    doc["val_tpl"] = "{{ value_json.running|default(false) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTWeightBasedDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/switch/cat_feeder/weight_based/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Weight Based", "cf_weight_based");
    doc["icon"] = "mdi:weight";
    doc["cmd_t"] = weightBasedCmdTopic;
    doc["payload_on"] = true;
    doc["payload_off"] = false;
    doc["val_tpl"] = "{{ value_json.weight_based|default(false) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTCloggedDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/binary_sensor/cat_feeder/clogged/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Clogged", "cf_clogged");
    doc["dev_cla"] = "problem";
    doc["payload_on"] = true;
    doc["payload_off"] = false;
    doc["val_tpl"] = "{{ value_json.clogged|default(false) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTFlowDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/number/cat_feeder/flow/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Revolution flow", "cf_flow");
    doc["icon"] = "mdi:fan-auto";
    doc["cmd_t"] = flowCmdTopic;
    doc["min"] = 0;
    doc["max"] = 100;
    doc["unit_of_meas"] = "g";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.flow|default(0) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTScaleZeroDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/number/cat_feeder/scale_zero/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Scale Zero", "cf_scale_zero");
    doc["icon"] = "mdi:fan-auto";
    doc["cmd_t"] = scaleZeroCmdTopic;
    doc["min"] = -1000;
    doc["max"] = 1000;
    doc["unit_of_meas"] = "g";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.scale_zero|default(0) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTClogToleranceDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/number/cat_feeder/clog_tolerance/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Clog Tolerance", "cf_clog_tolerance");
    doc["icon"] = "mdi:cookie-refresh-outline";
    doc["cmd_t"] = clogToleranceCmdTopic;
    doc["min"] = 0;
    doc["max"] = 100;
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.clog_tolerance|default(0) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTPullbackDegreesDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/number/cat_feeder/pullback_degrees/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Pullback Degrees", "cf_pullback_degrees");
    doc["icon"] = "mdi:skip-backward-outline";
    doc["cmd_t"] = pullbackDegreesCmdTopic;
    doc["min"] = 0;
    doc["max"] = 360;
    doc["unit_of_meas"] = "º";
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.pullback_degrees|default(0) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTLastDosisDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/sensor/cat_feeder/last_dosis/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Last Dosis", "cf_last_dosis");
    doc["icon"] = "mdi:food-drumstick-outline";
    doc["unit_of_meas"] = "g";
    doc["frc_upd"] = false;
    doc["val_tpl"] = "{{ value_json.last_dosis|default(0) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}

HassioMqttPayload HassioMqttContractBuilder::getMQTTSpeedDiscoveryPayload() {
    const char* discoveryTopic = "homeassistant/number/cat_feeder/speed/config";
    DynamicJsonDocument doc = buildDiscoveryStub("CF Speed", "cf_speed");
    doc["icon"] = "mdi:speedometer";
    doc["cmd_t"] = speedCmdTopic;
    doc["min"] = 0;
    doc["max"] = 100;
    doc["mode"] = "box";
    doc["val_tpl"] = "{{ value_json.speed|default(10) }}";
    return HassioMqttPayload(discoveryTopic, doc);
}
