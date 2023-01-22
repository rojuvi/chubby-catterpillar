#include <unity.h>
#include <HassioMqttContractBuilder.h>
#include <iostream>
#include <fstream>
#include <string>

double hardwareVersion = 1.0;
double softwareVersion = 2.0;
const char * author = "testAuthor";
const char * deviceId = "testDeviceId";
const char * deviceName = "testDevice";

const char * resourcesPath = "test/native/test_hassio_mqtt_handler/resources/";

HassioMqttContractBuilder hassioMqttConnectionManager(hardwareVersion, softwareVersion, author, deviceId, deviceName);

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

const char * readJson(const char * jsonPath) {
    std::ifstream jsonFile (jsonPath);
    std::string content((std::istreambuf_iterator<char>(jsonFile)), (std::istreambuf_iterator<char>()));
    return content.c_str();
}

void validatePayload(HassioMqttPayload payload, const char * expectedTopic, const char * messageJsonPath){
    TEST_ASSERT_EQUAL_STRING(expectedTopic, payload.topic);
    TEST_ASSERT_EQUAL_STRING(readJson(messageJsonPath), payload.buffer);
}

void test_getMQTTWeightDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTWeightDiscoveryPayload();
    validatePayload(payload, "homeassistant/sensor/cat_feeder/weight/config", 
        "test/native/test_hassio_mqtt_handler/resources/weightDiscoveryMessage.json");
    
}

void test_getMQTTAmountDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTAmountDiscoveryPayload();
    validatePayload(payload, "homeassistant/number/cat_feeder/amount/config", 
        "test/native/test_hassio_mqtt_handler/resources/amountDiscoveryMessage.json");
}

void test_getMQTTRunningDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTRunningDiscoveryPayload();
    validatePayload(payload, "homeassistant/switch/cat_feeder/running/config", 
        "test/native/test_hassio_mqtt_handler/resources/runningDiscoveryMessage.json");
}

void test_getMQTTWeightBasedDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTWeightBasedDiscoveryPayload();
    validatePayload(payload, "homeassistant/switch/cat_feeder/weight_based/config", 
        "test/native/test_hassio_mqtt_handler/resources/weightBasedDiscoveryMessage.json");
}

void test_getMQTTCloggedDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTCloggedDiscoveryPayload();
    validatePayload(payload, "homeassistant/binary_sensor/cat_feeder/clogged/config", 
        "test/native/test_hassio_mqtt_handler/resources/cloggedDiscoveryMessage.json");
}

void test_getMQTTFlowDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTFlowDiscoveryPayload();
    validatePayload(payload, "homeassistant/number/cat_feeder/flow/config", 
        "test/native/test_hassio_mqtt_handler/resources/flowDiscoveryMessage.json");
}

void test_getMQTTScaleZeroDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTScaleZeroDiscoveryPayload();
    validatePayload(payload, "homeassistant/number/cat_feeder/scale_zero/config", 
        "test/native/test_hassio_mqtt_handler/resources/scaleZeroDiscoveryMessage.json");
}

void test_getMQTTClogToleranceDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTClogToleranceDiscoveryPayload();
    validatePayload(payload, "homeassistant/number/cat_feeder/clog_tolerance/config", 
        "test/native/test_hassio_mqtt_handler/resources/clogToleranceDiscoveryMessage.json");
}

void test_getMQTTPullbackDegreesDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTPullbackDegreesDiscoveryPayload();
    validatePayload(payload, "homeassistant/number/cat_feeder/pullback_degrees/config", 
        "test/native/test_hassio_mqtt_handler/resources/pullbackDegreesDiscoveryMessage.json");
}

void test_getMQTTLastDosisDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTLastDosisDiscoveryPayload();
    validatePayload(payload, "homeassistant/sensor/cat_feeder/last_dosis/config", 
        "test/native/test_hassio_mqtt_handler/resources/lastDosisDiscoveryMessage.json");
}

void test_getMQTTSpeedDiscoveryPayload() {
    HassioMqttPayload payload = hassioMqttConnectionManager.getMQTTSpeedDiscoveryPayload();
    validatePayload(payload, "homeassistant/number/cat_feeder/speed/config", 
        "test/native/test_hassio_mqtt_handler/resources/speedDiscoveryMessage.json");
}

int main( int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_getMQTTWeightDiscoveryPayload);
    RUN_TEST(test_getMQTTAmountDiscoveryPayload);
    RUN_TEST(test_getMQTTRunningDiscoveryPayload);
    RUN_TEST(test_getMQTTWeightBasedDiscoveryPayload);
    RUN_TEST(test_getMQTTCloggedDiscoveryPayload);
    RUN_TEST(test_getMQTTFlowDiscoveryPayload);
    RUN_TEST(test_getMQTTScaleZeroDiscoveryPayload);
    RUN_TEST(test_getMQTTClogToleranceDiscoveryPayload);
    RUN_TEST(test_getMQTTPullbackDegreesDiscoveryPayload);
    RUN_TEST(test_getMQTTLastDosisDiscoveryPayload);
    RUN_TEST(test_getMQTTSpeedDiscoveryPayload);
    UNITY_END();
}