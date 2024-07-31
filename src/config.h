#ifndef _DD_CONFIG_H
#define _DD_CONFIG_H

// Watchdog
static const uint32_t WATCHDOG_DELAY_MS = 2000;
static const uint32_t WATCHDOG_TIMEOUT_MS = 8000; /* Do not exceed 8333 */

// Wifi
enum WifiMode {
  MQTT = 0,
  PING = 1,
  SCAN = 2,
};

static const enum WifiMode WIFI_MODE = MQTT;
static const uint32_t WIFI_TIMEOUT_MS = 7000; /* Ensure less than WATCHDOG_TIMEOUT_MS */
static const int32_t WIFI_CONNECT_RETRY_COUNT = 3;
static const bool PRINT_WIFI_CREDS = false;

// Blink Task
static const uint32_t BLINK_DELAY_MS = 500;
static const bool BLINK_DEBUG = 0;

// JSON
static const bool JSON_DEBUG = 0;

// FreeRTOS Resources
static const uint32_t MOTOR_QUEUE_DEPTH = 16;

#endif