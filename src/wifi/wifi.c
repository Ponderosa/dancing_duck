#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"

#define WIFI_SCAN_INTERVAL_MS (10000)
#define WIFI_SCAN_TASK_INTERVAL_MS (1000)

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
  if (result) {
    printf(
        "ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x "
        "sec: %u\n",
        result->ssid, result->rssi, result->channel, result->bssid[0],
        result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4],
        result->bssid[5], result->auth_mode);
  }
  return 0;
}

void vScanWifi() {
  absolute_time_t scan_time = nil_time;
  bool scan_in_progress = false;
  for (;;) {
    if (absolute_time_diff_us(get_absolute_time(), scan_time) < 0) {
      if (!scan_in_progress) {
        cyw43_wifi_scan_options_t scan_options = {0};
        int err =
            cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
        if (err == 0) {
          printf("\nPerforming wifi scan\n");
          scan_in_progress = true;
        } else {
          printf("Failed to start scan: %d\n", err);
          scan_time = make_timeout_time_ms(WIFI_SCAN_INTERVAL_MS);
        }
      } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
        scan_time = make_timeout_time_ms(WIFI_SCAN_INTERVAL_MS);
        scan_in_progress = false;
      }
    }
    vTaskDelay(WIFI_SCAN_TASK_INTERVAL_MS);
  }
}

#include "ping.h"

#define PING_ADDR ("8.8.8.8")

void vPing() {
  ip_addr_t ping_addr;
  ipaddr_aton(PING_ADDR, &ping_addr);
  ping_init(&ping_addr);

  while (true) {
    // not much to do as LED is in another task, and we're using RAW (callback)
    // lwIP API
    vTaskDelay(100);
  }
}
