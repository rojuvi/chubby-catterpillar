#include <unity.h>
#include <HassioMqttPayload.h>
#include <iostream>
#include <fstream>
#include <string>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_HassioMqttPayload_set() {
    const char* topic = "testTopic";
    DynamicJsonDocument message(64);
    message["test"] = "someValue";
    HassioMqttPayload payload = HassioMqttPayload(topic, message);
    TEST_ASSERT_EQUAL_STRING(topic, payload.topic);
    TEST_ASSERT_EQUAL_STRING("{\"test\":\"someValue\"}", payload.buffer);
    TEST_ASSERT_EQUAL_INT(20, payload.messageSize);
}

int main( int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_HassioMqttPayload_set);
    UNITY_END();
}