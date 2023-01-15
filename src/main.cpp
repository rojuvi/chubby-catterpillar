#include "../lib/config.h"
#include "../lib/hassio_mqtt_handler.h"
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HX711.h>

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


// Wifi config
IPAddress ip(192,168,1,142);     
IPAddress gateway(192,168,1,1);   
IPAddress subnet(255,255,255,0);
IPAddress dns1(192,168,1,1);
IPAddress dns2(1,1,1,1);
ESP8266WebServer server(80);

int hoursFrequency = 6;
int flow = AMT_PER_REV;
int amount = 25;
float numberOfRevolutions = 1.5;
int hours = 0;
int minutes = 0;
boolean isRunning = false;
int degreeSteps = STEPS/360;
int stepsPerLoop = degreeSteps;
int pullbackSteps = 20*degreeSteps;
int pullbackFrequency = 90*degreeSteps;
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

HassioMqttConnectionManager hassioManager = HassioMqttConnectionManager();

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

void endFeed() {
  Serial.println("Stop turning at steps: " + String(stepsCount));
  isRunning = false;
  hassioManager.publishStatus();
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
    hassioManager.publishStatus();
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
  if (hassioManager.runningCmdTopic == topic) {
    if (message == "True") {
      feed();
    } else {
      endFeed();
    }
  } else if (hassioManager.dosageCmdTopic == topic) {
    storeAmount(message.toInt());
    hassioManager.publishStatus();
  } else if (hassioManager.weightBasedCmdTopic == topic){
    storeWeightBased(message == "True");
    hassioManager.publishStatus();
  } else if (hassioManager.flowCmdTopic == topic) {
    storeFlow(message.toInt());
    hassioManager.publishStatus();
  } else if (hassioManager.scaleZeroCmdTopic == topic) {
    storeScaleZero(message.toInt());
    hassioManager.publishStatus();
  } else if (hassioManager.clogToleranceCmdTopic == topic) {
    storeClogTolerance(message.toInt());
    hassioManager.publishStatus();
  } else if (hassioManager.pullbackDegreesCmdTopic == topic) {
    storePullbackDegrees(message.toInt());
    hassioManager.publishStatus();
  } else if (hassioManager.speedCmdTopic == topic) {
    storeSpeed(message.toInt());
    hassioManager.publishStatus();
  } else {
    Serial.println("Invalid topic");
  }
  EEPROM.end();
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
  ptr +="<h2>Pullback Degrees: " + String(pullbackSteps/degreeSteps) + "deg</h2>\n";
  ptr +="<h2>Pullback Steps: " + String(pullbackSteps) + " steps</h2>\n";
  ptr +="<h2>Starting food: " + String(startingWeight) + "g</h2>\n";
  ptr +="<h2>Running food: " + String(runningWeight) + "g</h2>\n";
  ptr +="<h2>Remaining food: " + String(getWeight()) + "g</h2>\n";
  ptr +="<h2>Dosis: " + String(dosis) + "g</h2>\n";
  ptr +="<h2>Weight based: ";
  if (isWeightBased) {
    ptr +="<span>True</span>";
  } else {
    ptr +="<span>False</span>";
  }
  ptr +="</h2>\n";
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
  ptr +="<br />\n";
  ptr +="<label for=\"clog_tolerance\">Clog tolerance:</label>\n";
  ptr +="<input type=\"number\" min=\"1\"step=\"1\" name=\"clog_tolerance\" value=\"" + String(clog_tolerance) + "\">\n";
  ptr +="<label for=\"pullbackDegrees\">Pullback Degrees:</label>\n";
  ptr +="<input type=\"number\" min=\"1\"step=\"1\" name=\"pullbackDegrees\" value=\"" + String(pullbackSteps/degreeSteps) + "\">\n";
  ptr +="<label for=\"isWeightBased\">Weight based </label>\n";
  ptr +="<input type=\"checkbox\" name=\"isWeightBased\" ";
  if (isWeightBased) {
    ptr +="checked";
  }
  ptr += "/>\n";
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
        storeFlow(server.arg("flow").toInt());
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
        storeScaleZero(server.arg("scale_zero").toInt());
      }
      if(server.hasArg("scale_error_range")) {
        int tmp = server.arg("scale_error_range").toInt();
        if (tmp != scale_error_range) {
          scale_error_range = tmp;
          EEPROM.put(SCALE_ERROR_RANGE_ADDR, scale_error_range);
        }
      }
      if(server.hasArg("clog_tolerance")) {
        storeClogTolerance(server.arg("clog_tolerance").toInt());
      }
      if(server.hasArg("pullbackDegrees")) {
        int tmp = server.arg("pullbackDegrees").toInt()*degreeSteps;
        if (tmp != pullbackSteps) {
          pullbackSteps = tmp;
          EEPROM.put(PULLBACK_STEPS_ADDR, pullbackSteps);
        }
      }
      storeWeightBased(server.hasArg("isWeightBased"));
      EEPROM.end();
 
      String message = "<div>Body received:\n";
             message += "Frequency: " + server.arg("frequency") + "\n";
             message += "Amount: " + server.arg("amount") + "\n";
             message += "Flow: " + server.arg("flow") + "\n";
             message += "Hour: " + server.arg("hour") + "\n";
             message += "Min: " + server.arg("minutes") + "\n";
             message += "Scale zero: " + server.arg("scale_zeroutes") + "\n";
             message += "scale_error_range: " + server.arg("scale_error_range") + "\n";
             message += "clog_tolerance: " + server.arg("clog_tolerance") + "\n";
             message += "pullbackSteps: " + server.arg("pullbackSteps") + "\n";
             message += "isWeightBased: " + server.arg("isWeightBased") + "\n</div>";
             message += "\n";
             message += "<a href=\"/\">Go back</a>\n";

      server.sendHeader("Location", "/",true);
      server.send(302, "text/plane","");
//      server.send(200, "text/html", makeHTML(message));
      Serial.println(message);
      hassioManager.publishStatus();
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

  Serial.print("Frequency: ");
  Serial.println(hoursFrequency);
  Serial.print("Revolutions: ");
  Serial.println(numberOfRevolutions);

  // Scale init
  scale.begin(SCALE_DAT_PIN, SCALE_CLK_PIN);
  scale.set_scale(SCALE_CALIB_FACTOR);
  
  // MQTT init
  hassioManager.setup(mqttCallback);

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
    lastDosis = dosis;
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

void loop() {
  server.handleClient();

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
        runningWeight = getWeight();
        dosis = startingWeight-runningWeight;
        hassioManager.publishStatus();
        hassioManager.loop();
      }
      
      if (isFeedingEnd()) {
        endFeed();
      }
      if (stepsCount%pullbackFrequency == 0) {
        isPullBack = true;
      }
    }
  } else {
    digitalWrite(STEPPER_ENABLE_PIN, STEPPER_DISABLED);
    hassioManager.loop();
    unsigned long exTime = millis();
    if (exTime < hassioManager.lastMqttUpdateTime || exTime-hassioManager.lastMqttUpdateTime > MQTT_PERIODIC_UPDATE_INTERVAL) {
      hassioManager.setup(mqttCallback); // Just to ensure any server restarts prevent the CF from working
    }
  }
}
