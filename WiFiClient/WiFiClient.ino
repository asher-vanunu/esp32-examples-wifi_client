#include "Config.h"
#include "NetworkManager.h"
#include "MQTTManager.h"

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // 1. Setup Network and Captive Portal
  setupNetwork();

  // 2. Setup MQTT Client
  setupMQTT();
}

void loop() {
  checkResetButton();
  handleMQTT();
  publishTelemetry();
}