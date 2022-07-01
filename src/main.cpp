#include <config.h>
#include <Stepper.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HX711.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Stepper Constants
#define STEPS 2048
#define STEPS_4H 200
#define MAX_SPEED 10
#define MIN_SPEED 1

// EEPROM Constants
#define EEPROM_SIZE 40
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

// Time Constants
#define TIME_UPDATE_INTERVAL 60000
#define UTC_OFFSET_SEC 3600
#define AMT_PER_REV 16

// Scale Constants
#define SCALE_DAT_PIN D5
#define SCALE_CLK_PIN D6
#define SCALE_CALIB_FACTOR 466300.0


// MQTT Constants
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_PERIODIC_UPDATE_INTERVAL 60000


// Wifi config
IPAddress ip(192,168,1,142);     
IPAddress gateway(192,168,1,1);   
IPAddress subnet(255,255,255,0);
IPAddress dns1(192,168,1,1);
IPAddress dns2(1,1,1,1);
ESP8266WebServer server(80);

// Cat feeder config
Stepper stepper(STEPS, D0, D2, D1, D3);

int hoursFrequency = 6;
int flow = AMT_PER_REV;
int amount = 25;
float numberOfRevolutions = 1.5;
int hours = 0;
int minutes = 0;
boolean isRunning = false;
int stepsPerLoop = 8;
int pullbackSteps = 128;
int pullbackFrequency = 512;
int microPausa = 1000;
int dirPin = D1;
int stepPin = D0;

int stepsCount = 0;
boolean isPullBack = false;

int clog_tolerance = 3;

// Weight based dosage
boolean isWeightBased = true;
int startingWeight = 0;
int runningWeight = 0;
int dosis = 0;
int lastDosis = 0;
int scaleFrequency = 128;
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
const String dosageCmdTopic = "home/cat_feeder/dosage";
const String runningCmdTopic = "home/cat_feeder/running";
const String weightBasedCmdTopic = "home/cat_feeder/weight_based";
unsigned long lastMqttUpdateTime = 0;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

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
}

void sendMQTTDiscoveryMessage(String discoveryTopic, DynamicJsonDocument doc) {
  char buffer[MQTT_MAX_PACKET_SIZE];
  size_t n = serializeJson(doc, buffer);
  String message;
  for (int i = 0; i < n; i++) {
      message = message + buffer[i];  // convert *byte to string
  }
  Serial.print("Sending payload: ");
  Serial.println(message);
  Serial.println(client.publish(discoveryTopic.c_str(), buffer, n));
}

void sendMQTTWeightDiscoveryMessage() {
  String discoveryTopic = "homeassistant/sensor/cat_feeder/weight/config";
  DynamicJsonDocument doc(1024);
  doc["name"] = "CF remaining food";
  doc["icon"] = "mdi:food-drumstick";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "g";
  doc["frc_upd"] = false;
  doc["val_tpl"] = "{{ value_json.weight|default(0) }}";
  Serial.println("Send Weight discovery");
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTAmountDiscoveryMessage() {
  String discoveryTopic = "homeassistant/number/cat_feeder/amount/config";
  DynamicJsonDocument doc(1024);
  doc["name"] = "CF dosage";
  doc["icon"] = "mdi:weight-gram";
  doc["stat_t"] = stateTopic;
  doc["cmd_t"] = dosageCmdTopic;
  doc["min"] = 0;
  doc["max"] = 500;
  doc["unit_of_meas"] = "g";
  doc["val_tpl"] = "{{ value_json.dosage|default(0) }}";
  Serial.println("Send Amount discovery");
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTRunningDiscoveryMessage() {
  String discoveryTopic = "homeassistant/switch/cat_feeder/running/config";
  DynamicJsonDocument doc(1024);
  doc["name"] = "CF running";
  doc["icon"] = "mdi:food";
  doc["stat_t"] = stateTopic;
  doc["cmd_t"] = runningCmdTopic;
  doc["payload_on"] = true;
  doc["payload_off"] = false;
  doc["val_tpl"] = "{{ value_json.running|default(false) }}";
  Serial.println("Send Running discovery");
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTWeightBasedDiscoveryMessage() {
  String discoveryTopic = "homeassistant/switch/cat_feeder/weight_based/config";
  DynamicJsonDocument doc(1024);
  doc["name"] = "CF Weight Based";
  doc["icon"] = "mdi:weight";
  doc["stat_t"] = stateTopic;
  doc["cmd_t"] = weightBasedCmdTopic;
  doc["payload_on"] = true;
  doc["payload_off"] = false;
  doc["val_tpl"] = "{{ value_json.weight_based|default(false) }}";
  Serial.println("Send Weight Based discovery");
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMQTTCloggedDiscoveryMessage() {
  String discoveryTopic = "homeassistant/binary_sensor/cat_feeder/clogged/config";
  DynamicJsonDocument doc(1024);
  doc["name"] = "CF Clogged";
  doc["dev_cla"] = "problem";
  doc["stat_t"] = stateTopic;
  doc["payload_on"] = true;
  doc["payload_off"] = false;
  doc["val_tpl"] = "{{ value_json.clogged|default(false) }}";
  Serial.println("Send Clogged discovery");
  sendMQTTDiscoveryMessage(discoveryTopic, doc);
}

void sendMqttStatus() {
  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["weight"] = getWeight();
  doc["dosage"] = amount;
  doc["running"] = isRunning;
  doc["weight_based"] = isWeightBased;
  doc["clogged"] = isClogged;

  size_t n = serializeJson(doc, buffer);
  Serial.print("Sending mqtt status: ");
  String message;
  serializeJson(doc, message);
  Serial.println(message);

  bool published = client.publish(stateTopic.c_str(), buffer, n);
  lastMqttUpdateTime = millis();
}

void endFeed() {
  Serial.println("Stop turning at steps: " + String(stepsCount));
  isRunning = false;
  sendMqttStatus();
}

void feed() {
  if (!isRunning) {
    startingWeight = getWeight();
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

void storeAmount(int amt) {
  if (amt != amount) {
    amount = amt;
    EEPROM.put(AMOUNT_ADDR, amount);
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
  Serial.print(message);
  
  if (runningCmdTopic == topic) {
    if (message == "True") {
      feed();
    } else {
      endFeed();
    }
  } else if (dosageCmdTopic == topic) {
    int dosage = message.toInt();
    storeAmount(dosage);
    sendMqttStatus();
  } else if (weightBasedCmdTopic == topic){
     if (message == "True") {
      isWeightBased = true;
    } else {
      isWeightBased = false;
    }
    sendMqttStatus();
  } else {
    Serial.println("Invalid topic");
  }
}

void setupMqtt() {
  client.setBufferSize(MQTT_MAX_PACKET_SIZE);
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(mqttCallback);
  while (!client.connected()) {
    Serial.print(".");

    if (client.connect(mqttName.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("Connected to MQTT");
      sendMQTTAmountDiscoveryMessage();
      sendMQTTWeightDiscoveryMessage();
      sendMQTTRunningDiscoveryMessage();
      sendMQTTWeightBasedDiscoveryMessage();
      sendMQTTCloggedDiscoveryMessage();
      client.subscribe(dosageCmdTopic.c_str());
      client.subscribe(runningCmdTopic.c_str());
    } else {
      Serial.println("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
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
  Serial.print(twoDigit(minutes));

  if (!isRunning && hoursFrequency > 0 && hours != lastHourRun && minutes >= feedStartMinutes && (hours-feedStartHour)%hoursFrequency==0) {
    feed();
  }
  
}

String makeHTML(String body) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Cat Feeder Server</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr += body;
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

String homePage() {
  String ptr ="<div id=\"webpage\">\n";
  ptr +="<h1>Cat Feeder Server</h1>\n"; 
  
  ptr +="<div id=\"display\">\n";
  ptr +="<h2>Server time - " + twoDigit(hours) + ":" + twoDigit(minutes) + "</h2>\n";
  ptr +="<h2>Schedule start - " + twoDigit(feedStartHour) + ":" + twoDigit(feedStartMinutes) + "</h2>\n";
  ptr +="<h2>Last run - " + twoDigit(lastHourRun) + ":" + twoDigit(lastMinutesRun) + "</h2>\n";
  ptr +="<h2>Frequency: " + String(hoursFrequency) + "h</h2>\n";
  ptr +="<h2>Flow: " + String(flow) + "g/rev</h2>\n";
  ptr +="<h2>Amount: " + String(amount) + "g</h2>\n";
  ptr +="<h2>Number of revolutions: " + String(numberOfRevolutions) + "</h2>\n";
  ptr +="<h2>Scale zero: " + String(scale_zero) + "g</h2>\n";
  ptr +="<h2>Scale Error: +-" + String(scale_error_range) + "g</h2>\n";
  ptr +="<h2>Clog tolerance: " + String(clog_tolerance) + " times</h2>\n";
  ptr +="<h2>Pullback Steps: " + String(pullbackSteps) + " steps</h2>\n";
  ptr +="<h2>Starting food: " + String(startingWeight) + "g</h2>\n";
  ptr +="<h2>Running food: " + String(runningWeight) + "g</h2>\n";
  ptr +="<h2>Remaining food: " + String(getWeight()) + "g</h2>\n";
  ptr +="<h2>Dosis: " + String(dosis) + "g</h2>\n";
  ptr +="<h2>Running: ";
  if (isRunning) {
    ptr +="<span>True</span>";
  } else {
    ptr +="<span>False</span>";
  }
  ptr +="</h2>\n";
  ptr +="<h2>Clogged: ";
  if (isClogged) {
    ptr +="<span>True</span>";
  } else {
    ptr +="<span>False</span>";
  }
  ptr +="</h2>\n";
  ptr +="</div>\n";
  
  ptr +="<div id=\"config\">\n";
  ptr +="<form action=\"/config\">\n";
  ptr +="<label for=\"frequency\">Frequency (h)(0 to disable):</label>\n";
  ptr +="<input type=\"number\" min=\"0\" max=\"24\" step=\"1\" name=\"frequency\" value=\"" + String(hoursFrequency) + "\">\n";
  ptr +="<label for=\"amount\">Flow (g/rev):</label>\n";
  ptr +="<input type=\"number\" min=\"0\" step=\"1\" name=\"flow\" value=\"" + String(flow) + "\">\n";
  ptr +="<label for=\"amount\">Anount (g):</label>\n";
  ptr +="<input type=\"number\" min=\"0\" step=\"1\" name=\"amount\" value=\"" + String(amount) + "\">\n";
  ptr +="<br />\n";
  ptr +="<label for=\"hour\">Hour:</label>\n";
  ptr +="<input type=\"number\" min=\"0\" max=\"23\" step=\"1\" name=\"hour\" value=\"" + String(feedStartHour) + "\">\n";
  ptr +="<label for=\"minutes\">Minutes:</label>\n";
  ptr +="<input type=\"number\" min=\"0\" max=\"59\" step=\"1\" name=\"minutes\" value=\"" + String(feedStartMinutes) + "\">\n";
  ptr +="<br />\n";
  ptr +="<label for=\"scale_zero\">Scale Zero(g):</label>\n";
  ptr +="<input type=\"number\" step=\"1\" name=\"scale_zero\" value=\"" + String(scale_zero) + "\">\n";
  ptr +="<label for=\"scale_error_range\">Scale Error(g):</label>\n";
  ptr +="<input type=\"number\"  min=\"0\" step=\"1\" name=\"scale_error_range\" value=\"" + String(scale_error_range) + "\">\n";
  ptr +="<label for=\"clog_tolerance\">Clog tolerance:</label>\n";
  ptr +="<input type=\"number\" min=\"1\"step=\"1\" name=\"clog_tolerance\" value=\"" + String(clog_tolerance) + "\">\n";
  ptr +="<label for=\"pullbackSteps\">Pullback Steps:</label>\n";
  ptr +="<input type=\"number\" min=\"8\"step=\"8\" name=\"pullbackSteps\" value=\"" + String(pullbackSteps) + "\">\n";
  ptr +="<br />\n";
  ptr +="<button type=\"submit\">Update config</button>\n";
  ptr +="</form>\n";
  ptr +="</div>\n";

  ptr += "<a href=\"/run\">Run</a>";
  
  ptr +="</div>\n";
  return makeHTML(ptr);
}

void handle_OnConnect() {
  Serial.println("Connection in");
  server.send(200, "text/html", homePage());
}

void handle_OnRun() {
  feed();
  server.sendHeader("Location", "/",true);
  server.send(302, "text/plane","");
}

void handle_NotFound()
{
     server.send(404, "text/plain", "Ni idea de a dónde quieres ir pringao");
}

void handle_OnConfig() { //Handler for the body path
      if (!server.hasArg("frequency") && !server.hasArg("amount") 
          && !server.hasArg("hour") && !server.hasArg("minutes")){ //Check if body received
 
            server.send(200, "text/plain", "Body not received");
            return;
 
      }
      EEPROM.begin(EEPROM_SIZE);
  
      if(server.hasArg("frequency")) {
        int freq = server.arg("frequency").toInt();
        if (freq != hoursFrequency) {
          hoursFrequency = freq;
          EEPROM.put(FREQ_HOURS_ADDR, hoursFrequency);
        }
      }
      if(server.hasArg("amount")) {
        int amt = server.arg("amount").toInt();
        storeAmount(amt);
      }
      if(server.hasArg("flow")) {
        int flw = server.arg("flow").toInt();
        if (flw != flow) {
          flow = flw;
          EEPROM.put(FLOW_ADDR, flow);
        }
      }
      float nor = (float)amount/flow;
      if (nor != numberOfRevolutions) {
        numberOfRevolutions = nor;
        EEPROM.put(REVS_ADDR, numberOfRevolutions);
      }
      if(server.hasArg("hour")) {
        int tmp = server.arg("hour").toInt();
        if (tmp != feedStartHour) {
          feedStartHour = tmp;
          EEPROM.put(FEED_START_HOUR_ADDR, feedStartHour);
        }
      }
      if(server.hasArg("minutes")) {
        int tmp = server.arg("minutes").toInt();
        if (tmp != feedStartMinutes) {
          feedStartMinutes = tmp;
          EEPROM.put(FEED_START_MIN_ADDR, feedStartMinutes);
        }
      }
      if(server.hasArg("scale_zero")) {
        float tmp = server.arg("scale_zero").toFloat();
        if (tmp != scale_zero) {
          scale_zero = tmp;
          EEPROM.put(SCALE_FACTOR_ADDR, scale_zero);
        }
      }
      if(server.hasArg("scale_error_range")) {
        int tmp = server.arg("scale_error_range").toInt();
        if (tmp != scale_error_range) {
          scale_error_range = tmp;
          EEPROM.put(SCALE_ERROR_RANGE_ADDR, scale_error_range);
        }
      }
      if(server.hasArg("clog_tolerance")) {
        int tmp = server.arg("clog_tolerance").toInt();
        if (tmp != clog_tolerance) {
          clog_tolerance = tmp;
          EEPROM.put(CLOG_TOLERANCE_ADDR, clog_tolerance);
        }
      }
      if(server.hasArg("pullbackSteps")) {
        int tmp = server.arg("pullbackSteps").toInt();
        if (tmp != pullbackSteps) {
          pullbackSteps = tmp;
          EEPROM.put(PULLBACK_STEPS_ADDR, pullbackSteps);
        }
      }
      EEPROM.end();
 
      String message = "<div>Body received:\n";
             message += "Frequency: " + server.arg("frequency") + "\n";
             message += "Amount: " + server.arg("amount") + "\n</div>";
             message += "Flow: " + server.arg("flow") + "\n</div>";
             message += "Hour: " + server.arg("hour") + "\n</div>";
             message += "Min: " + server.arg("minutes") + "\n</div>";
             message += "Scale zero: " + server.arg("scale_zeroutes") + "\n</div>";
             message += "\n";
             message += "<a href=\"/\">Go back</a>\n";

      server.sendHeader("Location", "/",true);
      server.send(302, "text/plane","");
//      server.send(200, "text/html", makeHTML(message));
      Serial.println(message);
      sendMqttStatus();
}

void setup() {
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
  EEPROM.end();

  // Init wifi server
  Serial.println("Conectando ");
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

    // Set API handlers
  server.on("/", handle_OnConnect);
  server.on("/config", handle_OnConfig);
  server.on("/run", handle_OnRun);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

  // Set stepper motor
  stepper.setSpeed(MAX_SPEED);

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
  lastDosis = dosis;
}

boolean isFeedingEnd() {
  if (!isWeightBased) {
    return (float)stepsCount/STEPS >= numberOfRevolutions; 
  } else {
    return dosis >= amount;
  }
}

void doStep(int steps) {
  stepper.step(-steps);
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis-lastMillis >= TIME_UPDATE_INTERVAL) {
    lastMillis = currentMillis;
    checkTime();
  }
  
  if (isRunning) {
    if (isPullBack) {
      doStep(-pullbackSteps);
      doStep(pullbackSteps);
      isPullBack = false;
      detectClogging();
    } else {
      doStep(stepsPerLoop);
      stepsCount += stepsPerLoop;
      if (stepsCount % scaleFrequency == 0) {
        runningWeight = getWeight();
        dosis = startingWeight-runningWeight;
        sendMqttStatus();
        client.loop();
      }
      
      if (isFeedingEnd()) {
        endFeed();
      }
      if (stepsCount%pullbackFrequency == 0) {
        isPullBack = true;
      }
    }
  } else {
    client.loop();
    unsigned long exTime = millis();
    if (exTime < lastMqttUpdateTime || exTime-lastMqttUpdateTime > MQTT_PERIODIC_UPDATE_INTERVAL) {
      setupMqtt(); // Just to ensure any server restarts prevent the CF from working
    }
  }
}
