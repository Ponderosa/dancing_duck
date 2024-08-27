#ifndef _DD_CONFIG_H
#define _DD_CONFIG_H

// Firmware Version
static const uint32_t FIRMWARE_VERSION = 9;

// Watchdog
static const uint32_t WATCHDOG_DELAY_MS = 250;
static const uint32_t WATCHDOG_TIMEOUT_MS = 8000; /* Do not exceed 8333 */
static const uint32_t DD_MAGIC_NUM = 0xDECAFDAD;
static const bool DEBUG_IDLE = false;

// Wifi
enum WifiMode {
  MQTT = 0,
  PING = 1,
  SCAN = 2,
};

// Duck Mode
enum DuckMode {
  DRY_DOCK,
  CALIBRATE,
  LAUNCH,
  DANCE,
  OVERRIDE,
  STOP,
};

static const enum WifiMode WIFI_MODE = MQTT;
static const uint32_t WIFI_TIMEOUT_MS = 7000; /* Ensure less than WATCHDOG_TIMEOUT_MS */
static const int32_t WIFI_CONNECT_RETRY_COUNT = 5;
static const bool PRINT_WIFI_CREDS = false;

// Blink Task
static const uint32_t BLINK_DELAY_MS = 500;
static const bool BLINK_DEBUG = 0;

// JSON
static const bool JSON_DEBUG = 0;

// FreeRTOS Resources
static const uint32_t MOTOR_QUEUE_DEPTH = 16;

// Magnetometer
static const size_t KASA_ARRAY_DEPTH = 250;  // 25 Seconds
static const size_t KASA_LOOP_COUNTER = 25;  // 2.5 second
static const double KASA_RMSE_LOWER_LIMIT = 0.1;
static const double KASA_RMSE_UPPER_LIMIT = 10.0;
// Small value to check for near-zero conditions
static const double EPSILON = 1e-10;
static const uint32_t KASA_CALIBRATION_TIME_MS = 25000;

// Motor
static const double MIN_DUTY_CYCLE = 0.7;
static const double MAX_DUTY_CYCLE = 0.9;
static const double MID_DUTY_CYCLE = (MIN_DUTY_CYCLE + MAX_DUTY_CYCLE) / 2.0;
static const double Kp = 0.01;
static const double Kd = 0.001;

#endif