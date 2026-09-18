#include "ArduinoCommManager.h"
#include "Config.h"
#include <HardwareSerial.h>
#include <ArduinoJson.h>

// Forward declaration for publishing to MQTT
extern void publishNanoTelemetry(float tCol, float tSt, float tFlw, bool pumpOn, int state);

static HardwareSerial SerialNano(NANO_UART_NUM);
static unsigned long lastPollTime = 0;

void setupArduinoComm() {
  SerialNano.begin(NANO_BAUDRATE, SERIAL_8N1, NANO_RX_PIN, NANO_TX_PIN);
  Serial.println("[ArduinoComm] Serial channel to Arduino Nano initialized.");
}

void sendNanoMode(const char* mode) {
  if (strcmp(mode, "AUTO") == 0) {
    SerialNano.println("SET:MODE:AUTO");
  } else if (strcmp(mode, "MAN_ON") == 0) {
    SerialNano.println("SET:MODE:MAN_ON");
  } else if (strcmp(mode, "MAN_OFF") == 0) {
    SerialNano.println("SET:MODE:MAN_OFF");
  }
}

void sendNanoSetSMax(float smaxVal) {
  SerialNano.printf("SET:CFG:sMAX:%.1f\r\n", smaxVal);
}

void sendNanoSave() {
  SerialNano.println("CMD:SAVE");
}

static void parseNanoJSON(const String& jsonStr) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    return;
  }

  float tCol = doc["tCOL"] | 0.0f;
  float tSt  = doc["tST"]  | 0.0f;
  float tFlw = doc["tFLW"] | 0.0f;
  bool  pump = (doc["pump"] | 0) == 1;
  int  state = doc["state"] | 0;

  publishNanoTelemetry(tCol, tSt, tFlw, pump, state);
}

void handleArduinoComm() {
  // Poll data from Arduino Nano every 3 seconds
  unsigned long now = millis();
  if (now - lastPollTime > 3000) {
    lastPollTime = now;
    SerialNano.println("GET:STATUS");
    Serial.println("[ESP32 -> Nano] Sending: GET:STATUS");
  }

  // Read incoming serial data line by line
  static String inputBuffer = "";
  while (SerialNano.available()) {
    char c = (char)SerialNano.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        Serial.print("[ESP32 <- Nano] Received: ");
        Serial.println(inputBuffer);

        if (inputBuffer.startsWith("{") && inputBuffer.endsWith("}")) {
          parseNanoJSON(inputBuffer);
        }
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}