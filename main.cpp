#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- MACHINE ID ----------
#define MACHINE_ID "1" 

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- PINS ----------
#define PIN_LEFT    26
#define PIN_CONFIRM 25
#define PIN_RIGHT   33
#define PIN_COIN    27

// ---------- WIFI / MQTT ----------
const char* ssid = "MyPC";
const char* password = "00000000";
const char* mqtt_server = "192.168.1.50";

WiFiClient espClient;
PubSubClient client(espClient);

bool mqttEnabled = true; 

// ---------- EEPROM ----------
#define EEPROM_ADDR 0

// ---------- MENU ----------
enum MenuScreen {
  SCREEN_MAIN = 0,
  SCREEN_STATS,
  SCREEN_SETTINGS,
  SCREEN_RESET_CONFIRM,
  SCREEN_COUNT
};

uint8_t currentScreen = SCREEN_MAIN;

// ---------- STATE ----------
volatile uint8_t pulseCount = 0;
volatile unsigned long lastPulseTime = 0;
volatile bool coinInProgress = false;

float creditMAD = 0;
uint32_t totalCoins = 0;
float totalMAD = 0;

// Timers
unsigned long lastScreenUpdate = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastWiFiAttempt = 0;

// Simple button state
bool lastLeftState = HIGH;
bool lastRightState = HIGH;
bool lastConfirmState = HIGH;

// ----- FIXES -----
char mqttClientId[32];
bool dirty = false;
unsigned long lastSave = 0;
unsigned long lastBtnTime = 0;

// MQTT topic buffers
char topicBuffer[64];

// ---------- ISR ----------
void IRAM_ATTR coinISR() {
  static uint32_t last = 0;
  unsigned long now = millis();
  if (now - last > 5) {
    pulseCount++;
    lastPulseTime = now;
    coinInProgress = true;
    last = now;
  }
}

// ---------- COIN MAP ----------
float pulsesToMAD(uint8_t p) {
  switch (p) {
    case 1: return 0.5;
    case 2: return 1;
    case 3: return 2;
    case 4: return 5;
    case 5: return 10;
    default: return 0;
  }
}

// ---------- EEPROM ----------
void saveData() {
  EEPROM.put(EEPROM_ADDR, creditMAD);
  EEPROM.put(EEPROM_ADDR + 4, totalCoins);
  EEPROM.put(EEPROM_ADDR + 8, totalMAD);
  EEPROM.commit();
}

void loadData() {
  EEPROM.get(EEPROM_ADDR, creditMAD);
  EEPROM.get(EEPROM_ADDR + 4, totalCoins);
  EEPROM.get(EEPROM_ADDR + 8, totalMAD);
  
  if (isnan(creditMAD) || creditMAD < 0) creditMAD = 0;
  if (totalCoins > 1000000) totalCoins = 0;
  if (isnan(totalMAD) || totalMAD < 0) totalMAD = 0;
}

// ---------- OLED SCREENS ----------
void drawMainScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("== MACHINE ");
  display.print(MACHINE_ID);
  display.println(" ==");
  
  display.setTextSize(3);
  display.setCursor(5, 20);
  display.print(creditMAD, 1);
  
  display.setTextSize(2);
  display.setCursor(85, 28);
  display.print("MAD");
  
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(WiFi.status() == WL_CONNECTED ? "W" : ".");
  display.print(mqttEnabled && client.connected() ? "M" : ".");
  
  display.setCursor(30, 56);
  display.print("< MENU >");
  
  display.display();
}

void drawStatsScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("=== STATS ===");
  
  display.setCursor(0, 15);
  display.print("Total Coins:");
  display.setCursor(0, 25);
  display.print(totalCoins);
  
  display.setCursor(0, 40);
  display.print("Total MAD:");
  display.setCursor(0, 50);
  display.print(totalMAD, 1);
  
  display.setCursor(0, 56);
  display.print("< BACK | NEXT >");
  
  display.display();
}

void drawSettingsScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("=== SETTINGS ===");
  
  display.setCursor(0, 15);
  display.println("WiFi:");
  display.setCursor(0, 25);
  display.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  
  display.setCursor(0, 40);
  display.println("MQTT:");
  display.setCursor(0, 50);
  if (!mqttEnabled) {
    display.println("Disabled");
  } else {
    display.println(client.connected() ? "Connected" : "Disconnected");
  }
  
  display.setCursor(0, 56);
  display.print("< BACK | RESET >");
  
  display.display();
}

void drawResetConfirmScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("=== RESET ALL ===");
  
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println("RESET?");
  
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.println("This will clear");
  display.println("all data!");
  
  display.setCursor(0, 56);
  display.print("< NO | YES >");
  
  display.display();
}

void drawScreen() {
  switch (currentScreen) {
    case SCREEN_MAIN:
      drawMainScreen();
      break;
    case SCREEN_STATS:
      drawStatsScreen();
      break;
    case SCREEN_SETTINGS:
      drawSettingsScreen();
      break;
    case SCREEN_RESET_CONFIRM:
      drawResetConfirmScreen();
      break;
  }
}

// ---------- MQTT PUBLISH HELPERS ----------
void publishTopic(const char* subtopic, const char* payload) {
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/%s", MACHINE_ID, subtopic);
  client.publish(topicBuffer, payload);
}

void publishTopic(const char* subtopic, String payload) {
  publishTopic(subtopic, payload.c_str());
}

// ---------- WIFI (non-blocking) ----------
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
        snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/reset", MACHINE_ID);
        client.subscribe(topicBuffer);
        
        snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/start", MACHINE_ID);
        client.subscribe(topicBuffer);
        
        Serial.println("MQTT connected!");
        
        // Publish online status
        publishTopic("status", "online");
        publishTopic("credit", String(creditMAD));
      } else {
        Serial.println("MQTT failed");
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
  
  // Check if it's a reset command
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/reset", MACHINE_ID);
  if (String(topic) == String(topicBuffer)) {
    creditMAD = 0;
    totalCoins = 0;
    totalMAD = 0;
    dirty = true;
    publishTopic("credit", "0");
  }
  
  // Check if it's a start command (from payment app)
  snprintf(topicBuffer, sizeof(topicBuffer), "laundry/machine/%s/cmd/start", MACHINE_ID);
  if (String(topic) == String(topicBuffer)) {
    // Payment confirmed, you can add visual feedback here
    Serial.println("Start command received from payment app");
  }
}

// ---------- BUTTON HANDLERS ----------
void handleLeftButton() {
  Serial.println("LEFT!");
  if (currentScreen != SCREEN_MAIN) {
    currentScreen = (currentScreen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
  }
}

void handleRightButton() {
  Serial.println("RIGHT!");
  if (currentScreen == SCREEN_MAIN) {
    currentScreen = SCREEN_STATS;
  } else if (currentScreen == SCREEN_STATS) {
    currentScreen = SCREEN_SETTINGS;
  } else if (currentScreen == SCREEN_SETTINGS) {
    currentScreen = SCREEN_RESET_CONFIRM;
  } else if (currentScreen == SCREEN_RESET_CONFIRM) {
    currentScreen = SCREEN_MAIN;
  }
}

void handleConfirmButton() {
  Serial.println("CONFIRM!");
  if (currentScreen == SCREEN_MAIN) {
    if (creditMAD > 0) {
      // Publish payment request to console app
      char payload[32];
      snprintf(payload, sizeof(payload),
               "{\"machineId\":\"%s\",\"amount\":%.1f}",
               MACHINE_ID, creditMAD);

      publishTopic("payment/request", payload);
      
      creditMAD = 0;
      dirty = true;
      publishTopic("credit", "0");
    }
  } else if (currentScreen == SCREEN_RESET_CONFIRM) {
    creditMAD = 0;
    totalCoins = 0;
    totalMAD = 0;
    dirty = true;
    publishTopic("credit", "0");
    currentScreen = SCREEN_MAIN;
  } else {
    currentScreen = SCREEN_MAIN;
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("\n\nCOIN COLLECTOR STARTING...");
  Serial.print("Machine ID: ");
  Serial.println(MACHINE_ID);
  
  snprintf(mqttClientId, sizeof(mqttClientId),
           "ESP32_COIN_%s", MACHINE_ID);
  
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_CONFIRM, INPUT_PULLUP);
  pinMode(PIN_COIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_COIN), coinISR, FALLING);

  EEPROM.begin(32);
  loadData();

  Wire.begin(21, 22);
  delay(100);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    Serial.println(F("SSD1306 OK"));
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MACHINE ");
  display.println(MACHINE_ID);
  display.println("Starting...");
  display.display();
  delay(1000);

  WiFi.mode(WIFI_STA);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  Serial.println("Ready!");
}

// ---------- LOOP ----------
void loop() {
  
  // ----- BUTTONS FIRST -----
  bool leftNow = digitalRead(PIN_LEFT);
  bool rightNow = digitalRead(PIN_RIGHT);
  bool confirmNow = digitalRead(PIN_CONFIRM);
  
  if (millis() - lastBtnTime > 300) {
    if (leftNow == LOW && lastLeftState == HIGH) {
      handleLeftButton();
      lastBtnTime = millis();
    }

    if (rightNow == LOW && lastRightState == HIGH) {
      handleRightButton();
      lastBtnTime = millis();
    }

    if (confirmNow == LOW && lastConfirmState == HIGH) {
      handleConfirmButton();
      lastBtnTime = millis();
    }
  }
  
  lastLeftState = leftNow;
  lastRightState = rightNow;
  lastConfirmState = confirmNow;

  // ----- WiFi & MQTT -----
  serviceWiFi();
  serviceMQTT();

  // ----- Coin processing -----
  bool ready = false;
  noInterrupts();
  if (coinInProgress && millis() - lastPulseTime > 30) ready = true;
  interrupts();

  if (ready) {
    noInterrupts();
    uint8_t p = pulseCount;
    pulseCount = 0;
    coinInProgress = false;
    interrupts();

    float value = pulsesToMAD(p);
    if (value > 0) {
      creditMAD += value;
      totalCoins++;
      totalMAD += value;
      dirty = true;
      
      if (mqttEnabled && client.connected()) {
        publishTopic("coin/inserted", String(value));
        publishTopic("credit", String(creditMAD));
        publishTopic("stats/total", String(totalMAD));
      }
    }
  }

  // ----- Screen refresh -----
  if (millis() - lastScreenUpdate > 100) {
    drawScreen();
    lastScreenUpdate = millis();
  }
  
  if (dirty && millis() - lastSave > 3000) {
    saveData();
    lastSave = millis();
    dirty = false;
  }
}