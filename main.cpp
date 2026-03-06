#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>

// ---------- MACHINE ID ----------
#define MACHINE_ID "2" 

// ---------- PINS ----------
#define DEC_OPTO 16      // reset
#define INC_OPTO 17      // set
#define START_OPTO 5     // start
#define StateLEDINC 15
#define StateLEDDEC 2
#define StateLEDSTART 4
#define DOOR_SENSOR 18  

// ---------- WIFI / MQTT ----------
const char* ssid = "La_Fibre_dOrange_300A";
const char* password = "3A75Y2DZDKH67GXAEH";
const char* mqtt_server = "192.168.11.148";

WiFiClient espClient;
PubSubClient client(espClient);

bool mqttEnabled = true;
char mqttClientId[32];
char topicBuffer[64];

// ---------- EEPROM ----------
#define EEPROM_ADDR 0

// ---------- STATE ----------
int currentMode = 1;
bool machineRunning = false;
bool lastDoorState = HIGH;

// Timers
unsigned long lastMQTTAttempt = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastDoorCheck = 0;

// Timing
const unsigned long FAST_CYCLE = 50;

// ---------- EEPROM ----------
void saveCurrentMode() {
  EEPROM.put(EEPROM_ADDR, currentMode);
  EEPROM.commit();
}

void loadCurrentMode() {
  EEPROM.get(EEPROM_ADDR, currentMode);
  if (currentMode < 1 || currentMode > 8) currentMode = 1;
}

// ---------- MQTT PUBLISH HELPERS ----------
void publishTopic(const char* subtopic, const char* payload) {
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/%s", MACHINE_ID, subtopic);
  client.publish(topicBuffer, payload);
}

void publishTopic(const char* subtopic, String payload) {
  publishTopic(subtopic, payload.c_str());
}

// ---------- MACHINE CONTROL ----------
void pinON(int pin) {
  digitalWrite(pin, HIGH);
}

void pinOFF(int pin) {
  digitalWrite(pin, LOW);
}

void handleStart() {
  Serial.println("Starting machine");
  
  pinON(START_OPTO);
  pinON(StateLEDSTART);
  delay(FAST_CYCLE);
  pinOFF(START_OPTO);
  pinOFF(StateLEDSTART);
  delay(FAST_CYCLE);
  
  delay(500);
  
  if (client.connected()) {
    publishTopic("event/started", "1");
  }
}
void handleModeSelect(int targetMode) {
  Serial.print("Selecting mode: ");
  Serial.println(targetMode);

  // Phase 1: RESET
  for (int i = 0; i < 8; i++) {
    pinON(DEC_OPTO);
    pinON(StateLEDDEC);
    delay(FAST_CYCLE);
    pinOFF(DEC_OPTO);
    pinOFF(StateLEDDEC);
    delay(FAST_CYCLE);
  }

  // Phase 2: INCREMENT
  for (int i = 0; i < (targetMode - 1); i++) {
    pinON(INC_OPTO);
    pinON(StateLEDINC);
    delay(FAST_CYCLE);
    pinOFF(INC_OPTO);
    pinOFF(StateLEDINC);
    delay(FAST_CYCLE);
  }

  currentMode = targetMode;
  saveCurrentMode();
  
  if (client.connected()) {
    publishTopic("mode", String(currentMode));
  }

  // Auto start
  delay(2000);
  handleStart();
}

 // Optocoupler for door status (INPUT_PULLUP)
// ---------- DOOR SENSOR ----------
void checkDoorStatus() {
  if (millis() - lastDoorCheck < 500) return;
  lastDoorCheck = millis();
  
  bool doorState = digitalRead(DOOR_SENSOR);
  
  if (doorState != lastDoorState) {
    lastDoorState = doorState;
    
    if (doorState == HIGH) {
      machineRunning = true;
      Serial.println("Machine RUNNING (door closed)");
      if (client.connected()) {
        publishTopic("door", "closed");
        publishTopic("running", "1");
      }
    } else {
      machineRunning = false;
      Serial.println("Machine STOPPED (door open)");
      if (client.connected()) {
        publishTopic("door", "open");
        publishTopic("running", "0");
        publishTopic("event/finished", "1");
      }
    }
  }
}

// ---------- WIFI ----------
void serviceWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastWiFiAttempt > 10000) {
    lastWiFiAttempt = millis();
    Serial.println("Attempting WiFi...");
    WiFi.begin(ssid, password);
  }
}

// ---------- MQTT ----------
void serviceMQTT() {
  if (!mqttEnabled) return;
  
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastMQTTAttempt > 10000) {
      lastMQTTAttempt = millis();
      Serial.println("Attempting MQTT...");
      
      if (client.connect(mqttClientId)) {
        // Subscribe to machine-specific commands
        snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/setmode", MACHINE_ID);
        client.subscribe(topicBuffer);
        
        snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/start", MACHINE_ID);
        client.subscribe(topicBuffer);
        
        snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/reset", MACHINE_ID);
        client.subscribe(topicBuffer);
        
        Serial.println("MQTT connected!");
        
        // Publish current state
        publishTopic("status", "online");
        publishTopic("mode", String(currentMode));
        publishTopic("running", machineRunning ? "1" : "0");
        publishTopic("door", machineRunning ? "closed" : "open");
      } else {
        Serial.print("MQTT failed, rc=");
        Serial.println(client.state());
      }
    }
  }
  
  if (client.connected()) {
    client.loop();
  }
}

// ---------- MQTT CALLBACK ----------
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // Handle mode selection
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/setmode", MACHINE_ID);
  if (String(topic) == String(topicBuffer)) {
    int mode = message.toInt();
    if (mode >= 1 && mode <= 8) {
      handleModeSelect(mode);
    }
  }
  
  // Handle start command
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/start", MACHINE_ID);
  if (String(topic) == String(topicBuffer)) {
    handleStart();
  }
  
  // Handle reset
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/reset", MACHINE_ID);
  if (String(topic) == String(topicBuffer)) {
    handleModeSelect(1);
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);
  Serial.println("\n\nMACHINE CONTROLLER STARTING...");
  Serial.print("Machine ID: ");
  Serial.println(MACHINE_ID);
  
  snprintf(mqttClientId, sizeof(mqttClientId), "ESP32_CTRL_%s", MACHINE_ID);
  
  pinMode(DEC_OPTO, OUTPUT);
  pinMode(INC_OPTO, OUTPUT);
  pinMode(START_OPTO, OUTPUT);
  pinMode(StateLEDINC, OUTPUT);
  pinMode(StateLEDDEC, OUTPUT);
  pinMode(StateLEDSTART, OUTPUT);
  pinMode(DOOR_SENSOR, INPUT_PULLUP);
  
  pinOFF(StateLEDINC);
  pinOFF(StateLEDDEC);
  pinOFF(StateLEDSTART);
  pinOFF(DEC_OPTO);
  pinOFF(INC_OPTO);
  pinOFF(START_OPTO);
  
  EEPROM.begin(32);
  loadCurrentMode();
  
  WiFi.mode(WIFI_STA);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  Serial.println("Ready!");
}

// ---------- LOOP ----------
void loop() {
  serviceWiFi();
  serviceMQTT();
  checkDoorStatus();
  delay(10);
}