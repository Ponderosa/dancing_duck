#ifndef _DD_CONFIG_H
#define _DD_CONFIG_H

#ifndef WIFI_MODE
#define WIFI_MODE MQTT
#endif

enum wifi_mode {
  MQTT = 0,
  PING = 1,
  SCAN = 2,
};

// Uncomment to print WiFi creds for debugging
// Do not commit uncommented!
// #define PRINT_WIFI_CREDS

#define MOTOR_QUEUE_DEPTH 16

#endif