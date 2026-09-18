#include "MQTTManager.h"
#include "Config.h"
#include "ModbusManager.h"
#include "ArduinoCommManager.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

void publishPumpTelemetry(bool isRunning, uint16_t powerW, uint16_t flowM3H, float energyKWh) {
  if (!mqttClient.connected()) return;

  char strBuffer[16];
  mqttClient.publish("pool/pump/state", isRunning ? "ON" : "OFF");

  snprintf(strBuffer, sizeof(strBuffer), "%u", powerW);
  mqttClient.publish("pool/pump/power_watts", strBuffer);

  snprintf(strBuffer, sizeof(strBuffer), "%u", flowM3H);
  mqttClient.publish("pool/pump/flow_m3h", strBuffer);

  snprintf(strBuffer, sizeof(strBuffer), "%.2f", energyKWh);
  mqttClient.publish("pool/pump/energy_kwh", strBuffer);
}

void publishNanoTelemetry(float tCol, float tSt, float tFlw, bool pumpOn, int state) {
  if (!mqttClient.connected()) return;

  char strBuffer[16];

  snprintf(strBuffer, sizeof(strBuffer), "%.1f", tCol);
  mqttClient.publish("pool/heating/tCOL", strBuffer);

  snprintf(strBuffer, sizeof(strBuffer), "%.1f", tSt);
  mqttClient.publish("pool/heating/tST", strBuffer);

  snprintf(strBuffer, sizeof(strBuffer), "%.1f", tFlw);
  mqttClient.publish("pool/heating/tFLW", strBuffer);

  mqttClient.publish("pool/heating/pump_state", pumpOn ? "ON" : "OFF");

  snprintf(strBuffer, sizeof(strBuffer), "%d", state);
  mqttClient.publish("pool/heating/state", strBuffer);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[32];
  if (length >= sizeof(message)) length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("[MQTT] Message arrived [%s]: %s\n", topic, message);

  // Pump control
  if (strcmp(topic, "pool/pump/set_state") == 0) {
    setPumpPowerState(strcmp(message, "ON") == 0);
  } 
  else if (strcmp(topic, "pool/pump/set_flow") == 0) {
    uint16_t targetFlow = atoi(message);
    setPumpFlowRate(targetFlow);
  }
  // Heating controller commands (Arduino Nano)
  else if (strcmp(topic, "pool/heating/set_mode") == 0) {
    sendNanoMode(message);
  }
  else if (strcmp(topic, "pool/heating/set_smax") == 0) {
    float smaxVal = atof(message);
    sendNanoSetSMax(smaxVal);
    sendNanoSave();
  }
}

static void reconnectMQTT() {
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  if (now - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = now;

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[MQTT] Attempting connection...");
      if (mqttClient.connect("Pool_ESP32_Master", "pool/status", 1, true, "offline")) {
        Serial.println(" Connected!");
        mqttClient.publish("pool/status", "online", true);

        mqttClient.subscribe("pool/pump/set_state");
        mqttClient.subscribe("pool/pump/set_flow");
        mqttClient.subscribe("pool/heating/set_mode");
        mqttClient.subscribe("pool/heating/set_smax");
      } else {
        Serial.printf(" Failed, rc=%d\n", mqttClient.state());
      }
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
  } else {
    mqttClient.loop();
  }
}