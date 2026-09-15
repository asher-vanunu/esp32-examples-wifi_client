#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>

// Initialize MQTT client configuration
void setupMQTT();

// Maintain MQTT connection and process incoming/outgoing packets
void handleMQTT();

// Publish telemetry/sensor data
void publishTelemetry();

#endif