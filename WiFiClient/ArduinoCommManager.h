#ifndef ARDUINO_COMM_MANAGER_H
#define ARDUINO_COMM_MANAGER_H

#include <Arduino.h>

void setupArduinoComm();
void handleArduinoComm();

// Functions to send commands down to Arduino Nano
void sendNanoMode(const char* mode);
void sendNanoSetSMax(float smaxVal);
void sendNanoSave();

#endif