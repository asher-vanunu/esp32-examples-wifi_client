#include "MQTTManager.h"
#include "Config.h"

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static unsigned long lastMsgTime = 0;

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  if (String(topic) == "pool/pump/set_state") {
    if (message == "ON") {
      digitalWrite(RELAY_PUMP_PIN, HIGH);
      mqttClient.publish("pool/pump/state", "ON");
    } else if (message == "OFF") {
      digitalWrite(RELAY_PUMP_PIN, LOW);
      mqttClient.publish("pool/pump/state", "OFF");
    }
  }
}

static void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (mqttClient.connect("Pool_ESP32_Master", "pool/status", 1, true, "offline")) {
      Serial.println("connected!");
      mqttClient.publish("pool/status", "online", true);
      mqttClient.subscribe("pool/pump/set_state");
    } else {
      Serial.printf("failed, rc=%d. Retrying in 5 seconds...\n", mqttClient.state());
      delay(5000);
    }
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void handleMQTT() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
}

void publishTelemetry() {
  unsigned long now = millis();
  if (now - lastMsgTime > 10000) {
    lastMsgTime = now;
    
    float temp = 26.5;
    mqttClient.publish("pool/temperature/water", String(temp, 1).c_str());
  }
}