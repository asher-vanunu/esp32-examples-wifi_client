//const char* WIFI_SSID     = "cellcom-va2.4";     // שם רשת ה-Wi-Fi
//const char* WIFI_PASSWORD = "av046080021"; // סיסמת ה-Wi-Fi
//const char* ssid = "cellcom-va2.4";
//const char* password = "av046080021"; // הסיסמה לרשת ה-Wi-Fi

#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiManager.h> // Provisioning library for interactive Wi-Fi setup
#include <PubSubClient.h>

// --- MQTT Server Settings ---
const char* mqtt_server = "10.100.102.27";    // ה-IP של השרת שבו רץ ה-Docker
const int mqtt_port = 1883;

// --- Pin Definitions ---
const int RELAY_PUMP_PIN = 26; // Output relay pin for pump/equipment control
const int BOOT_BUTTON_PIN = 0;  // Built-in BOOT button on ESP32 DevBoard (GPIO 0)

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiManager wm;

// Global variables for timing and button state tracking
unsigned long lastMsgTime = 0;
unsigned long buttonPressStart = 0;
bool buttonStatePrevious = HIGH;

// Set Wi-Fi regional parameters to enable channels 1-13 (Israel/Europe standard)
void configureWiFiCountry() {
  WiFi.mode(WIFI_STA);
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);

  wifi_country_t country = {
    .cc = "IL",
    .schan = 1,
    .nchan = 13,
    .max_tx_power = 20,
    .policy = WIFI_COUNTRY_POLICY_AUTO
  };
  esp_wifi_set_country(&country);
}

// Callback function to process incoming MQTT command payloads from Home Assistant
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  // Handle pool pump activation/deactivation commands
  if (String(topic) == "pool/pump/set_state") {
    if (message == "ON") {
      digitalWrite(RELAY_PUMP_PIN, HIGH);
      mqttClient.publish("pool/pump/state", "ON"); // Confirm state back to Home Assistant
    } else if (message == "OFF") {
      digitalWrite(RELAY_PUMP_PIN, LOW);
      mqttClient.publish("pool/pump/state", "OFF");
    }
  }
}

// Reconnect loop for MQTT broker with LWT (Last Will and Testament) integration
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Connect with LWT payload to notify HA if device goes offline unexpectedly
    if (mqttClient.connect("Pool_ESP32_Master", "pool/status", 1, true, "offline")) {
      Serial.println("connected!");
      
      // Publish online availability status
      mqttClient.publish("pool/status", "online", true);

      // Subscribe to control topics from Home Assistant
      mqttClient.subscribe("pool/pump/set_state");
    } else {
      Serial.printf("failed, rc=%d. Retrying in 5 seconds...\n", mqttClient.state());
      delay(5000);
    }
  }
}

// Monitor the BOOT button for a long-press (3 seconds) to clear Wi-Fi credentials
void checkResetButton() {
  bool buttonStateCurrent = digitalRead(BOOT_BUTTON_PIN);

  // Detect button press start (Active LOW)
  if (buttonStateCurrent == LOW && buttonStatePrevious == HIGH) {
    buttonPressStart = millis();
  }

  // Check if held for more than 3000 milliseconds
  if (buttonStateCurrent == LOW && (millis() - buttonPressStart > 3000)) {
    Serial.println("\n[RESET] BOOT Button held for 3s! Clearing Wi-Fi credentials...");
    wm.resetSettings(); // Erase stored Wi-Fi credentials from NVS Flash
    delay(1000);
    ESP.restart(); // Reboot ESP32 to open Captive Portal again
  }

  buttonStatePrevious = buttonStateCurrent;
}

void setup() {
  Serial.begin(115200);

  // Initialize GPIO pins
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // 1. Configure Wi-Fi regional settings
  configureWiFiCountry();

  // 2. Configure WiFiManager Captive Portal
  wm.setConfigPortalTimeout(180); // 3-minute timeout for configuration portal

  // Automatically connect using stored credentials, or launch "Pool-Controller-AP" portal if not configured
  bool res = wm.autoConnect("Pool-Controller-AP");

  if (!res) {
    Serial.println("Failed to connect or hit timeout. Restarting...");
    ESP.restart();
  } else {
    Serial.println("\nWiFi Connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  // 3. Initialize MQTT Client configuration
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  // Continuously check for BOOT button long-press
  checkResetButton();

  // Maintain MQTT broker connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Periodically send telemetry data (e.g., sensor readings) every 10 seconds
  unsigned long now = millis();
  if (now - lastMsgTime > 10000) {
    lastMsgTime = now;
    
    float temp = 26.5; // Placeholder value for temperature sensor
    mqttClient.publish("pool/temperature/water", String(temp, 1).c_str());
  }
}