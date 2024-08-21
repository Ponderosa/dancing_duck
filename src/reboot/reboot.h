#ifndef _DD_REBOOT_H
#define _DD_REBOOT_H

enum rebootReasonSoft {
  NO_SOFT_REASON,
  WIFI_INIT_REASON,
  WIFI_CONNECT_REASON,
  MQTT_PUBLISH_TIMEOUT_REASON,
  MQTT_COMMANDED_REASON,
  TEST_REASON,
};

enum rebootReasonHard {
  NO_HARD_REASON = 0,
  WATCHDOG_REASON = 1,
  HAD_POR = 2,
  HAD_RUN = 3,
  HAD_OTHER = 4,
};

void on_boot();
void reboot(enum rebootReasonSoft reason);
enum rebootReasonSoft rebootReasonSoft();
enum rebootReasonHard rebootReasonHard();
uint32_t bootCount();

#endif