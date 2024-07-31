#ifndef _DD_CONFIG_H
#define _DD_CONFIG_H

// Watchdog
#define WATCHDOG_DELAY_MS   2000
#define WATCHDOG_TIMEOUT_MS 8000 /*Do not exceed 8333*/

// Wifi
enum WifiMode {
  MQTT = 0,
  PING = 1,
  SCAN = 2,
};

#define WIFI_MODE                MQTT
#define WIFI_TIMEOUT_MS          7000 /*Ensure less than WATCHDOG_TIMEOUT_MS*/
#define WIFI_CONNECT_RETRY_COUNT 3
static const bool PRINT_WIFI_CREDS = false;

// Blink Task
#define BLINK_DELAY_MS    500
#define BLINK_DEBUG       0

// JSON
#define JSON_DEBUG        0

// FreeRTOS Resources
#define MOTOR_QUEUE_DEPTH 16

#endif