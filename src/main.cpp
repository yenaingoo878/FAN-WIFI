#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "ButtonInput.h"
#include <ESPAsyncWiFiManager.h>

// Pins
#define POWER_PIN   0
#define SPEED1_PIN  1
#define SPEED2_PIN  2
#define SPEED3_PIN  3

// Buttons
ButtonInput BUTTON_POWER(4);
ButtonInput BUTTON_1(5);
ButtonInput BUTTON_2(6);
ButtonInput BUTTON_3(7);

// Global variables
AsyncWebServer server(80);
WebSocketsServer webSocket(81);

Preferences preferences;
DNSServer dns;
AsyncWiFiManager wifiManager(&server, &dns);

bool currentPower = false;
int currentFanSpeed = 0;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;
unsigned long lastStatusUpdate = 0;
const unsigned long statusUpdateInterval = 3000;

// ========================== Function Prototypes ==========================
void setupWiFi();
void setFanSpeed(int speed);
void checkSwitchState();
void sendStatusUpdate();
String GetCurrentStatus();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

// ========================== Setup ==========================
void setup() {
  Serial.begin(115200);

  pinMode(POWER_PIN, OUTPUT);
  pinMode(SPEED1_PIN, OUTPUT);
  pinMode(SPEED2_PIN, OUTPUT);
  pinMode(SPEED3_PIN, OUTPUT);

  BUTTON_POWER.begin();
  BUTTON_1.begin();
  BUTTON_2.begin();
  BUTTON_3.begin();

  preferences.begin("FanPrefs", false);

  setupWiFi();

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed!");
    return;
  }

  int savedSpeed = preferences.getInt("lastSpeed", 0);
  setFanSpeed(savedSpeed);

  // Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/css/style.css", "text/css");
  });

  server.on("/script/socket.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/script/socket.js", "text/javascript");
  });

  server.on("/images/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/images/logo.svg", "image/svg+xml");
  });

  server.on("/images/power.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/images/power.svg", "image/svg+xml");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Page Not Found");
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ========================== Loop ==========================
void loop() {
  BUTTON_POWER.update();
  BUTTON_1.update();
  BUTTON_2.update();
  BUTTON_3.update();

  checkSwitchState();
  webSocket.loop();
  sendStatusUpdate();
}

// ========================== WiFi Setup ==========================
void setupWiFi() {
  WiFi.mode(WIFI_STA);

  if (!WiFi.SSID().isEmpty()) {
    Serial.println("📶 Connecting to saved WiFi...");
    WiFi.begin();
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Failed. Starting WiFiManager AP mode...");
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.startConfigPortal("SmartFan-Setup");
  }
  
  // Start mDNS
  if (!MDNS.begin("IOFAN")) {
    Serial.println("Error starting mDNS");
    return;
  }
  Serial.println("mDNS responder started: iofan.local");

  // Advertise WebSocket service
  MDNS.addService("ws", "tcp", 81); // _ws._tcp.local
  
  Serial.println("Connected to WiFi!");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());
}

// ========================== Fan Control ==========================
void setFanSpeed(int speed) {
  digitalWrite(SPEED1_PIN, LOW);
  digitalWrite(SPEED2_PIN, LOW);
  digitalWrite(SPEED3_PIN, LOW);

  if (speed == 0) {
    digitalWrite(POWER_PIN, LOW);
    currentPower = false;
  } else {
    digitalWrite(POWER_PIN, HIGH);
    currentPower = true;
    switch (speed) {
      case 1: digitalWrite(SPEED1_PIN, HIGH); break;
      case 2: digitalWrite(SPEED2_PIN, HIGH); break;
      case 3: digitalWrite(SPEED3_PIN, HIGH); break;
    }
  }

  currentFanSpeed = speed;
  preferences.putInt("lastSpeed", speed);
  sendStatusUpdate();
}

// ========================== Button Handler ==========================
void checkSwitchState() {
  if (millis() - lastButtonPress < debounceDelay) return;

  int newSpeed = currentFanSpeed;

  if (BUTTON_3.DoubleClick() && BUTTON_3.IsReallyPressed()) {
    Serial.println("🔄 BUTTON3 Long Press Detected - Resetting WiFi...");
    preferences.clear();
    WiFi.disconnect(true, true);
    ESP.restart();
    return; // ESP restarted, no need to continue
  }
  
       if (BUTTON_1.DoubleClick() && BUTTON_1.IsReallyPressed()) newSpeed = 0;
  else if (BUTTON_1.SingleClick() && BUTTON_1.IsReallyPressed()) newSpeed = 1;
  else if (BUTTON_2.SingleClick() && BUTTON_2.IsReallyPressed()) newSpeed = 2;
  else if (BUTTON_3.SingleClick() && BUTTON_3.IsReallyPressed()) newSpeed = 3;

  if (newSpeed != currentFanSpeed) {
    lastButtonPress = millis();
    setFanSpeed(newSpeed);
  }
}

// ========================== WebSocket ==========================
void sendStatusUpdate() {
  if (millis() - lastStatusUpdate > statusUpdateInterval) {
    lastStatusUpdate = millis();
    String status = GetCurrentStatus();
    webSocket.broadcastTXT(status);
  }
}

String GetCurrentStatus() {
  JsonDocument doc;
  doc["power"] = currentPower;
  doc["speed"] = currentFanSpeed;
  doc["mac"] = WiFi.macAddress();
  String output;
  serializeJson(doc, output);
  return output;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected\n", num);
      webSocket.sendTXT(num, "Connected");
      sendStatusUpdate();
      break;

    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected\n", num);
      break;

    case WStype_TEXT: {
      String message = String((char *)payload);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, message);
      if (error) return;

      if (doc["POWER"].is<bool>()) {
        bool power = doc["POWER"];
        int speed = doc["SPEED"] | currentFanSpeed;
        setFanSpeed(power ? speed : 0);
      }
    } break;
  }
}