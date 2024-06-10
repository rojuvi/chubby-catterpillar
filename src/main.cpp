#include <config.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HX711.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <map>

#define HW_VERSION 2.0
#define VERSION 2.1
#define AUTHOR "donkikote"
#define DEVICE_ID "cat_feeder"
#define DEVICE_NAME "Cat Feeder"

// Stepper Constants
#define STEPS 3200
#define STEP_DEFAULT_DELAY 1500
#define DIR_PIN D0
#define STEP_PIN D1
#define STEPPER_ENABLE_PIN D2
#define M1 D6
#define M2 D7
#define M3 D8
#define STEPPER_ENABLED LOW
#define STEPPER_DISABLED HIGH
#define CLOCKWISE HIGH
#define COUNTER_CLOCKWISE LOW

// EEPROM Constants
#define EEPROM_SIZE 48
#define FREQ_HOURS_ADDR 0
#define REVS_ADDR 4
#define FEED_START_HOUR_ADDR 8
#define FEED_START_MIN_ADDR 12
#define AMOUNT_ADDR 16
#define FLOW_ADDR 20
#define SCALE_FACTOR_ADDR 24
#define CLOG_TOLERANCE_ADDR 28
#define SCALE_ERROR_RANGE_ADDR 32
#define PULLBACK_STEPS_ADDR 36
#define WEIGHT_BASED_ADDR 40
#define SPEED_ADDR 44

// Time Constants
#define TIME_UPDATE_INTERVAL 60000
#define UTC_OFFSET_SEC 3600
#define AMT_PER_REV 16

// Scale Constants
#define SCALE_DAT_PIN D3
#define SCALE_CLK_PIN D4
#define SCALE_CALIB_FACTOR 466300.0
#define ACCURATE_WEIGHT_MEASURES 10


// MQTT Constants
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_PERIODIC_UPDATE_INTERVAL 2000


// Wifi config
IPAddress ip(192,168,1,142);     
IPAddress gateway(192,168,1,1);   
IPAddress subnet(255,255,255,0);
IPAddress dns1(192,168,1,1);
IPAddress dns2(1,1,1,1);

int hoursFrequency = 6;
int flow = AMT_PER_REV;
int amount = 25;
float numberOfRevolutions = 1.5;
int hours = 0;
int minutes = 0;
boolean isRunning = false;
int degreeSteps = STEPS/360;
int stepsPerLoop = 15*degreeSteps;
int pullbackSteps = 90*degreeSteps;
int pullbackFrequency = 180*degreeSteps;
int speed = 10;
int stepDelay = STEP_DEFAULT_DELAY;

int stepsCount = 0;
boolean isPullBack = false;

int clog_tolerance = 3;

// Weight based dosage
boolean isWeightBased = true;
int startingWeight = 0;
int runningWeight = 0;
int dosis = 0;
int lastDosis = 0;
int scaleFrequency = 90*degreeSteps;
boolean isClogged = false;
int clogDetectedTimes = 0;

// Time settings
int feedStartHour = 0;
int feedStartMinutes = 0;
int lastHourRun = -1;
int lastMinutesRun = -1;
unsigned long lastMillis;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", UTC_OFFSET_SEC);

// Scale config
HX711 scale;
float scale_calibration = 466300.0;
int scale_zero = -288;
int scale_error_range = 1;

// MQTT Config
const String mqttName = "Cat Feeder";
const String stateTopic = "home/cat_feeder/state";
const String availabilityTopic = "home/cat_feeder/available";
const String dosageCmdTopic = "home/cat_feeder/dosage";
const String runningCmdTopic = "home/cat_feeder/running";
const String weightBasedCmdTopic = "home/cat_feeder/weight_based";
const String flowCmdTopic = "home/cat_feeder/flow";
const String scaleZeroCmdTopic = "home/cat_feeder/scale_zero";
const String clogToleranceCmdTopic = "home/cat_feeder/clog_tolerance";
const String pullbackDegreesCmdTopic = "home/cat_feeder/pullback_degrees";
const String speedCmdTopic = "home/cat_feeder/speed";
unsigned long lastMqttUpdateTime = 0;
WiFiClient wifiClient;
PubSubClient client(wifiClient);
DynamicJsonDocument deviceInfo(1024);

void setupMqtt(); // Forward declaration

String twoDigit(int val) {
  String out = "";
  if(val < 10){
     out += '0';
  }
  out += val;
  return out;
}

int getWeight() {
  return (int)(scale.get_units()*1000)-scale_zero;
  // return 0;
}

int getAccurateWeight() {
  int maxCount = 0;
  int mode = 0;
  std::map<int,int> measures;
  for (int i=0;i<ACCURATE_WEIGHT_MEASURES;i++) {
      int measure = getWeight();
      measures[measure]++;
      if (measures[measure] > maxCount) {
        maxCount = measures[measure];
        mode = measure;
      }
      delay(100);
  }
  return mode;
}

void sendMQTTDiscoveryMessage(String discoveryTopic, DynamicJsonDocument doc) {
  char buffer[MQTT_MAX_PACKET_SIZE];
  size_t n = serializeJson(doc, buffer);
  String message;
  for (int i = 0; i < n; i++) {
      message = message + buffer[i];  // convert *byte to string
  }
  // Serial.print("Sending discovery: ");
  // Serial.println(message);
  client.publish(discoveryTopic.c_str(), buffer, n);
}

DynamicJsonDocument buildDiscoveryStub(String name, String id) {
  DynamicJsonDocument doc(1024);
  doc["name"] = name;
  doc["uniq_id"] = id;
  doc["stat_t"] = stateTopic;
  doc["device"] = deviceInfo;
  doc["avty_t"] = availabilityTopic;
  return doc;
}

void sendMQTTWeightDiscoveryMessage() {
  String discoveryTopic = "homeassistant/sensor/cat_feeder/weight/config";
  DynamicJsonDocument doc = buildDiscoveryStub("CF remaining food", "cf_weight");
  doc["icon"] = "mdi:food-drumstick";
  doc["unit_of_meas"] = "g";
  doc["frc_upd"] = false;
  doc["val_tpl"] = "{{ value_json.weight|default(0) }}";
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTAmountDiscoveryMessage() {
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

void sendMQTTRunningDiscoveryMessage() {
  String discoveryTopic = "homeassistant/switch/cat_feeder/running/config";
  DynamicJsonDocument doc = buildDiscoveryStub("CF running", "cf_running");
  doc["icon"] = "mdi:food";
  doc["cmd_t"] = runningCmdTopic;
  doc["payload_on"] = true;
  doc["payload_off"] = false;
  doc["val_tpl"] = "{{ value_json.running|default(false) }}";
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTWeightBasedDiscoveryMessage() {
  String discoveryTopic = "homeassistant/switch/cat_feeder/weight_based/config";
  DynamicJsonDocument doc = buildDiscoveryStub("CF Weight Based", "cf_weight_based");
  doc["icon"] = "mdi:weight";
  doc["cmd_t"] = weightBasedCmdTopic;
  doc["payload_on"] = true;
  doc["payload_off"] = false;
  doc["val_tpl"] = "{{ value_json.weight_based|default(false) }}";
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTCloggedDiscoveryMessage() {
  String discoveryTopic = "homeassistant/binary_sensor/cat_feeder/clogged/config";
  DynamicJsonDocument doc = buildDiscoveryStub("CF Clogged", "cf_clogged");
  doc["dev_cla"] = "problem";
  doc["payload_on"] = true;
  doc["payload_off"] = false;
  doc["val_tpl"] = "{{ value_json.clogged|default(false) }}";
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTFlowDiscoveryMessage() {
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

void sendMQTTScaleZeroDiscoveryMessage() {
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

void sendMQTTClogToleranceDiscoveryMessage() {
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

void sendMQTTPullbackDegreesDiscoveryMessage() {
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

void sendMQTTLastDosisDiscoveryMessage() {
  String discoveryTopic = "homeassistant/sensor/cat_feeder/last_dosis/config";
  DynamicJsonDocument doc = buildDiscoveryStub("CF Last Dosis", "cf_last_dosis");
  doc["icon"] = "mdi:food-drumstick-outline";
  doc["unit_of_meas"] = "g";
  doc["frc_upd"] = false;
  doc["val_tpl"] = "{{ value_json.last_dosis|default(0) }}";
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTSpeedDiscoveryMessage() {
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

void sendMqttStatus(float weight) {
  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["weight"] = weight;
  doc["dosage"] = amount;
  doc["running"] = isRunning;
  doc["weight_based"] = isWeightBased;
  doc["clogged"] = isClogged;
  doc["flow"] = flow;
  doc["scale_zero"] = scale_zero;
  doc["clog_tolerance"] = clog_tolerance;
  doc["pullback_degrees"] = pullbackSteps/degreeSteps;
  doc["last_dosis"] = lastDosis;
  doc["speed"] = speed;

  size_t n = serializeJson(doc, buffer);
  String message;
  serializeJson(doc, message);
  // Serial.println(message);

  client.publish(stateTopic.c_str(), buffer, n);
  Serial.println("Done");
  lastMqttUpdateTime = millis();
}

void sendMqttStatus() {
  sendMqttStatus(getAccurateWeight());
}

void doStep(int steps, bool clockwise) {
  if (clockwise) {
    digitalWrite(DIR_PIN, CLOCKWISE);
  } else {
    digitalWrite(DIR_PIN, COUNTER_CLOCKWISE);
  }
  for (int x = 0; x < steps * 1; x++) {
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(stepDelay);
      digitalWrite(STEP_PIN, LOW);
      delayMicroseconds(stepDelay);
   }
}

void push(int steps) {
  doStep(steps, false);
}

void pull(int steps) {
  doStep(steps, true);
}

void endFeed() {
  pull(pullbackSteps*2);
  Serial.println("Stop turning at steps: " + String(stepsCount));
  isRunning = false;
  lastDosis = startingWeight-getAccurateWeight();
  sendMqttStatus();
}

void feed() {
  if (!isRunning) {
    startingWeight = getAccurateWeight();
    runningWeight = startingWeight;
    dosis = 0;
    lastDosis = 0;
    stepsCount = 0;
    lastHourRun = hours;
    lastMinutesRun = minutes;
    isRunning = true;

    clogDetectedTimes = 0;
    sendMqttStatus();
  }
}

void storeAmount(int val) {
  if (val != amount) {
    amount = val;
    EEPROM.put(AMOUNT_ADDR, amount);
  }
}

void storeFlow(int val){
  if (val != flow) {
    flow = val;
    EEPROM.put(FLOW_ADDR, flow);
  }
}

void storeWeightBased(bool val) {
  if (val != isWeightBased) {
    isWeightBased = val;
    EEPROM.put(WEIGHT_BASED_ADDR, isWeightBased);
  }
}

void storeScaleZero(int val) {
  if (val != scale_zero) {
    scale_zero = val;
    EEPROM.put(SCALE_FACTOR_ADDR, scale_zero);
  }
}

void storeClogTolerance(int val) {
  if (val != clog_tolerance) {
    clog_tolerance = val;
    EEPROM.put(CLOG_TOLERANCE_ADDR, clog_tolerance);
  }
}

void storePullbackDegrees(int degrees) {
  int steps = degrees*degreeSteps;
  if (steps != pullbackSteps) {
    pullbackSteps = steps;
    EEPROM.put(PULLBACK_STEPS_ADDR, pullbackSteps);
  }
}

void storeSpeed(int val) {
  if (val != speed) {
    speed = val;
    EEPROM.put(SPEED_ADDR, speed);
    stepDelay = STEP_DEFAULT_DELAY/speed*10;
  }
}

void handleHassStatusChange(String message) {
  if (message == MQTT_ONLINE) {
      setupMqtt();
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length){
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  String message;
  for (int i = 0; i < length; i++) {
      message = message + (char) payload[i];  // convert *byte to string
  }
  Serial.println(message);
  EEPROM.begin(EEPROM_SIZE);
  if (runningCmdTopic == topic) {
    if (message == "True") {
      feed();
    } else {
      endFeed();
    }
  } else if (dosageCmdTopic == topic) {
    storeAmount(message.toInt());
    sendMqttStatus();
  } else if (weightBasedCmdTopic == topic){
    storeWeightBased(message == "True");
    sendMqttStatus();
  } else if (flowCmdTopic == topic) {
    storeFlow(message.toInt());
    sendMqttStatus();
  } else if (scaleZeroCmdTopic == topic) {
    storeScaleZero(message.toInt());
    sendMqttStatus();
  } else if (clogToleranceCmdTopic == topic) {
    storeClogTolerance(message.toInt());
    sendMqttStatus();
  } else if (pullbackDegreesCmdTopic == topic) {
    storePullbackDegrees(message.toInt());
    sendMqttStatus();
  } else if (speedCmdTopic == topic) {
    storeSpeed(message.toInt());
    sendMqttStatus();
  } else if (MQTT_HASS_STATUS_TOPIC == topic) {
    handleHassStatusChange(message);
  } else {
    Serial.println("Invalid topic");
  }
  EEPROM.end();
}

bool setOnline() {
  return client.publish(availabilityTopic.c_str(), MQTT_ONLINE, true);
}

void setupMqtt() {
  Serial.print("Setting up mqtt.");
  client.setBufferSize(MQTT_MAX_PACKET_SIZE);
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(mqttCallback);
  deviceInfo["hw_version"] = HW_VERSION;
  deviceInfo["sw_version"] = VERSION;
  deviceInfo["identifiers"] = DEVICE_ID;
  deviceInfo["manufacturer"] = AUTHOR;
  deviceInfo["name"] = DEVICE_NAME;
  while (!client.connected()) {
    Serial.print(".");

    if (client.connect(mqttName.c_str(), MQTT_USER, MQTT_PASS, availabilityTopic.c_str(), 0, true, MQTT_OFFLINE)) {
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
      client.subscribe(dosageCmdTopic.c_str());
      client.subscribe(runningCmdTopic.c_str());
      client.subscribe(weightBasedCmdTopic.c_str());
      client.subscribe(flowCmdTopic.c_str());
      client.subscribe(scaleZeroCmdTopic.c_str());
      client.subscribe(clogToleranceCmdTopic.c_str());
      client.subscribe(pullbackDegreesCmdTopic.c_str());
      client.subscribe(speedCmdTopic.c_str());
      
      client.subscribe(MQTT_HASS_STATUS_TOPIC.c_str());
    } else {
      Serial.println("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  Serial.println("");
  Serial.println("Connected to MQTT");
  setOnline();
  sendMqttStatus();
}

void checkTime() {
  timeClient.update();

  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  Serial.print(daysOfTheWeek[timeClient.getDay()]);
  Serial.print(", ");
  Serial.print(twoDigit(hours));
  Serial.print(":");
  Serial.println(twoDigit(minutes));

  if (!isRunning && hoursFrequency > 0 && hours != lastHourRun && minutes >= feedStartMinutes && (hours-feedStartHour)%hoursFrequency==0) {
    feed();
  }
  
}

void setup() {
  // Set stepper motor
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(STEPPER_ENABLE_PIN, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  pinMode(M3, OUTPUT);
  digitalWrite(STEPPER_ENABLE_PIN, STEPPER_DISABLED);
  // Set microstepping https://www.diarioelectronicohoy.com/blog/descripcion-del-driver-a4988
  digitalWrite(M1, HIGH);
  digitalWrite(M2, HIGH);
  digitalWrite(M3, HIGH);

  Serial.begin(9600);

  // Load programmable data from eeprom
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(FREQ_HOURS_ADDR, hoursFrequency);
  EEPROM.get(REVS_ADDR, numberOfRevolutions);
  EEPROM.get(FEED_START_HOUR_ADDR, feedStartHour);
  EEPROM.get(FEED_START_MIN_ADDR, feedStartMinutes);
  EEPROM.get(AMOUNT_ADDR, amount);
  EEPROM.get(FLOW_ADDR, flow);
  EEPROM.get(SCALE_FACTOR_ADDR, scale_zero);
  EEPROM.get(CLOG_TOLERANCE_ADDR, clog_tolerance);
  EEPROM.get(SCALE_ERROR_RANGE_ADDR, scale_error_range);
  EEPROM.get(PULLBACK_STEPS_ADDR, pullbackSteps);
  EEPROM.get(WEIGHT_BASED_ADDR, isWeightBased);
  EEPROM.get(SPEED_ADDR, speed);
  EEPROM.end();
  stepDelay = STEP_DEFAULT_DELAY/speed*10;

  // Init wifi server
  Serial.println("Connecting");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, dns1, dns2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {  
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("IP: "); 
  Serial.println(WiFi.localIP());

  Serial.print("Frequency: ");
  Serial.println(hoursFrequency);
  Serial.print("Revolutions: ");
  Serial.println(numberOfRevolutions);

  // Scale init
  scale.begin(SCALE_DAT_PIN, SCALE_CLK_PIN);
  scale.set_scale(SCALE_CALIB_FACTOR);
  
  // MQTT init
  setupMqtt();

  // Time init
  timeClient.begin();
  
  checkTime();
}

void detectClogging() {
  // same or lower value X number of times
  if (isWeightBased) {
    Serial.print("Dosis/LastDosis: ");
    Serial.print(dosis);
    Serial.print("/");
    Serial.println(lastDosis);
    if (abs(dosis-lastDosis)<=scale_error_range) {
      clogDetectedTimes++;
      if (clogDetectedTimes >= clog_tolerance) {
        isClogged = true;
        endFeed();
      }
    } else {
      clogDetectedTimes = 0;
      isClogged = false;
    }
  } else {
    isClogged = false;
  }  
}

boolean isFeedingEnd() {
  if (!isWeightBased) {
    return (float)stepsCount/STEPS >= numberOfRevolutions; 
  } else {
    return dosis >= amount;
  }
}

boolean isReallyFeedingEnd() {
  if (!isWeightBased) {
    return true;
  } else {
    dosis = startingWeight-getAccurateWeight();
    return isFeedingEnd();
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis-lastMillis >= TIME_UPDATE_INTERVAL) {
    lastMillis = currentMillis;
    checkTime();
  }
  
  if (isRunning) {
    digitalWrite(STEPPER_ENABLE_PIN, STEPPER_ENABLED);
    if (isPullBack) {
      Serial.print("Start pullback");
      Serial.println(pullbackSteps);
      pull(pullbackSteps);
      push(pullbackSteps);
      isPullBack = false;
      detectClogging();
      Serial.println("End pullback");
    } else {
      push(stepsPerLoop);
      stepsCount += stepsPerLoop;

      if (stepsCount % scaleFrequency == 0) {
        runningWeight = getAccurateWeight();
      } else {
        runningWeight = getWeight();
      }
      dosis = startingWeight-runningWeight;
      sendMqttStatus(runningWeight);
      client.loop();
      
      if (isFeedingEnd() && isReallyFeedingEnd()) {
        endFeed();
      }
      if (stepsCount%pullbackFrequency == 0) {
        isPullBack = true;
      }
    }
  } else {
    digitalWrite(STEPPER_ENABLE_PIN, STEPPER_DISABLED);
    client.loop();
    unsigned long exTime = millis();
    if (exTime < lastMqttUpdateTime || exTime-lastMqttUpdateTime > MQTT_PERIODIC_UPDATE_INTERVAL) {
      sendMqttStatus();
    }
  }
}
