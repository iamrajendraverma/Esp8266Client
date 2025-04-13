#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <pins_arduino.h> // Added this line
#include <EEPROM.h>
#include <PubSubClient.h>

// EEPROM addresses
#define EEPROM_IDENTIFIER 20
#define EEPROM_WIFI_SSID_ADDR 0
#define EEPROM_WIFI_PASSWORD_ADDR 64
#define EEPROM_MQTT_SERVER_ADDR 128
#define EEPROM_MQTT_PORT_ADDR 192
#define EEPROM_MQTT_USER_ADDR 256
#define EEPROM_MQTT_PASSWORD_ADDR 320

#define d1 5
#define d2 4
#define d0 16
#define d4 2
#define d5 14

boolean pirEnable = false;
int pirState = LOW;
int previousPirState = LOW;
bool motionDetected = false;
unsigned long lastDebounceTime = 0;
int  debounceDelay = 200;

#define FLASH_BUTTON_PIN 0 // Change to your flash button pin
// WiFi AP credentials
const char *ssidAP = "ESP8266-Config";
const char *passwordAP = "password"; // Set a secure password

// Web server and WebSocket server
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
WiFiClient espClient;
PubSubClient client(espClient);

// Configuration variables
String identifier = "";
String wifiSSID = "";
String wifiPassword = "";
String mqttServer = "";
String mqttPort = "";
String mqttUser = "";
String mqttPassword = "";

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload, length);
    if (doc.containsKey("identifier"))
      identifier = doc["identifier"].as<String>();
    if (doc.containsKey("wifiSSID")) {
      wifiSSID = doc["wifiSSID"].as<String>();
    }
    if (doc.containsKey("wifiPassword")) {
      wifiPassword = doc["wifiPassword"].as<String>();
    }
    if (doc.containsKey("mqttServer")) {
      mqttServer = doc["mqttServer"].as<String>();
    }
    if (doc.containsKey("mqttPort")) {
      mqttPort = doc["mqttPort"].as<String>();
    }
    if (doc.containsKey("mqttUser")) {
      mqttUser = doc["mqttUser"].as<String>();
    }
    if (doc.containsKey("mqttPassword")) {
      mqttPassword = doc["mqttPassword"].as<String>();
    }

    if (!wifiSSID.isEmpty() && !wifiPassword.isEmpty() && !mqttServer.isEmpty() && !mqttPort.isEmpty()) {
      Serial.println("Received configuration:");
      Serial.print("identifier: ");
      Serial.println(identifier);
      Serial.print("WiFi SSID: ");
      Serial.println(wifiSSID);
      Serial.print("WiFi Password: ");
      Serial.println(wifiPassword);
      Serial.print("MQTT Server: ");
      Serial.println(mqttServer);
      Serial.print("MQTT Port: ");
      Serial.println(mqttPort);
      Serial.print("MQTT User: ");
      Serial.println(mqttUser);
      Serial.print("MQTT Password: ");
      Serial.println(mqttPassword);

      webSocket.broadcastTXT("Config received");
      saveConfigToEEPROM();

      connectWiFiAndMQTT();
    }
  }
}


void connectWiFiAndMQTT() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifiSSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  connectMQTT();
}

void connectMQTT() {
  client.setServer(mqttServer.c_str(), mqttPort.toInt());
  client.setCallback(callback);
  String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
  DynamicJsonDocument doc(1024);
  doc["clientID"] = clientId;
  doc["identifier"] = identifier;
  doc["address"] = WiFi.macAddress();
  doc["IP-address"] = WiFi.localIP().toString();
  doc["connected"] = false;
  std::string json = docToString(doc); // Convert doc to string

  if (client.connect(clientId.c_str(), "esp/status", 0, true, json.c_str())) {
    Serial.println("Connected to MQTT");
    doc["connected"] = true;
    std::string json = docToString(doc); // Convert doc to string

    client.publish("esp/status", json.c_str()); // Initial online message
    String macAddress = WiFi.macAddress();
    Serial.println("Mac Address is subscribed");
    client.subscribe(macAddress.c_str());
    // ...
  } else {
    Serial.print("MQTT connection failed, rc=");
    Serial.println(client.state());
    Serial.println("Retrying MQTT connection in 5 seconds...");
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload, length); // Check for errors

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return; // Exit the function if deserialization failed
  }
  // Serial.println("Print Payload");
  // printPayload(doc);

  boolean action = false;
  String pin = "";

  if (doc.containsKey("pin")) {
    pin = doc["pin"].as<String>();
  }
  
    
  if(doc.containsKey("frequency")){
    if (doc["frequency"].is<int>()) {
     debounceDelay = doc["frequency"].as<int>();
     
  }
  }

  if (doc.containsKey("action")) {
    if (doc["action"].is<boolean>()) {
      action = doc["action"].as<boolean>();
    } else if (doc["action"].is<const char *>()) {
      String tempAction = doc["action"].as<const char *>();
      if (tempAction.equalsIgnoreCase("true")) {
        action = true;
      } else {
        action = false;
      }
    }
  }

  switch (pin.charAt(1)) { // Check the second character ('1' or '2')
  case '1':
    Serial.println("Pin is D1");
    digitalWrite(d1, action ? LOW : HIGH);
    break;
  case '2':
    Serial.println("Pin is D2");
    digitalWrite(d2, action ? LOW : HIGH);
    break;
  case '3': // Handle D0
    Serial.println("Pin is D0");
    digitalWrite(d0, action ? LOW : HIGH);
    break;
  case '4':
    Serial.println("Pin is D4");
    digitalWrite(d4, action ? LOW : HIGH);
    break;
  case '5':

  pirEnable = action;
  

   break;
  default:
    Serial.println("Pin is neither D1,D2,D0 nor D4");
    break;
  }
  String macAddress = WiFi.macAddress();
  String res = macAddress + "/response";
  doc["address"] = macAddress;
  std::string json = docToString(doc); // Convert doc to string

  client.publish(res.c_str(), json.c_str());
}

void printPayload(const JsonDocument& jsonDoc) {
  Serial.println("--- Printing JSON Payload ---");
  serializeJsonPretty(jsonDoc, Serial); // Use serializeJsonPretty for formatted output
  Serial.println();
  Serial.println("--- End of Payload ---");
}

void handleRoot() {
  server.send(200, "text/html", "<p>Configuration portal</p>");
}
void setup() {
  pinMode(d1, OUTPUT);
  pinMode(d2, OUTPUT);
  pinMode(d0, OUTPUT);
  pinMode(d4, OUTPUT);
  pinMode(d5,INPUT);
  // INITIAL VALUE IS FALSE (HIGH for active low LEDs)
  digitalWrite(d1, HIGH);
  digitalWrite(d2, HIGH);
  digitalWrite(d0, HIGH);
  digitalWrite(d4, HIGH);


  Serial.begin(115200);
  pinMode(FLASH_BUTTON_PIN, INPUT_PULLUP); // Set flash button pin as input pullup

  loadConfigFromEEPROM(); // Load settings from EEPROM

  Serial.println("Wait for Reset the configuration");
  delay(5000);
  if (digitalRead(FLASH_BUTTON_PIN) == LOW) { // Check if flash button is pressed
    Serial.println("Flash button pressed. Starting config AP.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidAP, passwordAP);

    IPAddress myIP = WiFi.softAPIP();
    Serial.println("AP IP address: ");
    Serial.println(myIP);

    server.on("/", handleRoot);
    server.begin();

    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);
    Serial.println("WebSocket server started");
  } else {
    Serial.println("Flash button not pressed. Connecting using saved config.");
    if (!wifiSSID.isEmpty() && !wifiPassword.isEmpty() && !mqttServer.isEmpty() && !mqttPort.isEmpty()) {
      connectWiFiAndMQTT();
    } else {
      Serial.println("No saved config, starting AP mode");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssidAP, passwordAP);
      IPAddress myIP = WiFi.softAPIP();
      Serial.println("AP IP address: ");
      Serial.println(myIP);
      server.on("/", handleRoot);
      server.begin();
      webSocket.begin();
      webSocket.onEvent(handleWebSocketMessage);
      Serial.println("WebSocket server started");
    }
  }
}

void loop() {
  server.handleClient();
  webSocket.loop();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      connectMQTT();
    }
    
    client.loop();
    if(client.connected() && pirEnable){
      pir_sensor();

    }
  } else {
    Serial.println("WiFi Disconnected. Attempting to reconnect...");
    WiFi.reconnect();
    // Optionally add a delay here
  }
}
void pir_sensor(){
    DynamicJsonDocument doc(1024);
    int currentPirState = digitalRead(d5);
    String macAddress = WiFi.macAddress();
  String res = macAddress + "/response";
  doc["address"] = macAddress;
  doc["data-type"] ="sensor";
  doc["sensor"] = "pir";
  Serial.println("debounceDelay");
  Serial.println(debounceDelay);
  if (currentPirState != previousPirState) {
    if (millis() - lastDebounceTime > debounceDelay) {
      if (currentPirState == HIGH) {
      Serial.println("Motion detected");

      doc["action"] = true;
      doc["debounceDelay"] = debounceDelay;

      std::string json = docToString(doc); 
      client.publish(res.c_str(), json.c_str());
      } else {
        doc["action"] = false;
              doc["debounceDelay"] = debounceDelay;

      std::string json = docToString(doc); 
      client.publish(res.c_str(), json.c_str());

      }
      previousPirState = currentPirState;

    }
    lastDebounceTime = millis();

  }

  delay(50); // Small delay
}

void saveConfigToEEPROM() {
  EEPROM.begin(512);
  identifier.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_IDENTIFIER), 64);
  wifiSSID.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_WIFI_SSID_ADDR), 64);
  wifiPassword.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_WIFI_PASSWORD_ADDR), 64);
  mqttServer.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_MQTT_SERVER_ADDR), 64);
  mqttPort.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_MQTT_PORT_ADDR), 64);
  mqttUser.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_MQTT_USER_ADDR), 64);
  mqttPassword.toCharArray((char *)(EEPROM.getDataPtr() + EEPROM_MQTT_PASSWORD_ADDR), 64);

  EEPROM.commit();
  EEPROM.end();
  Serial.println("Config saved to EEPROM\n");
}

void loadConfigFromEEPROM() {
  EEPROM.begin(512);
  identifier = String((const char *)(EEPROM.getDataPtr() + EEPROM_IDENTIFIER));
  wifiSSID = String((const char *)(EEPROM.getDataPtr() + EEPROM_WIFI_SSID_ADDR));
  wifiPassword = String((const char *)(EEPROM.getDataPtr() + EEPROM_WIFI_PASSWORD_ADDR));
  mqttServer = String((const char *)(EEPROM.getDataPtr() + EEPROM_MQTT_SERVER_ADDR));
  mqttPort = String((const char *)(EEPROM.getDataPtr() + EEPROM_MQTT_PORT_ADDR));
  mqttUser = String((const char *)(EEPROM.getDataPtr() + EEPROM_MQTT_USER_ADDR));
  mqttPassword = String((const char *)(EEPROM.getDataPtr() + EEPROM_MQTT_PASSWORD_ADDR));

  EEPROM.end();
  Serial.println("-----------------------------------");
  Serial.println("Config loaded from EEPROM");
}

std::string docToString(DynamicJsonDocument &doc) {
  std::string jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}