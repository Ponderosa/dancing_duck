#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"

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

#include "hardware/clocks.h"
#include "hardware/vreg.h"

void vBlinkTask() {
  for (;;) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    printf("Blink On\n");

    vTaskDelay(500);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    printf("Blink Off\n");

    vTaskDelay(500);
  }
}

void vScanWifi() {
  absolute_time_t scan_time = nil_time;
  bool scan_in_progress = false;
  while (true) {
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
          scan_time = make_timeout_time_ms(10000);  // wait 10s and scan again
        }
      } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
        scan_time = make_timeout_time_ms(10000);  // wait 10s and scan again
        scan_in_progress = false;
      }
    }
    vTaskDelay(1000);
  }
}

void main() {
  stdio_init_all();

  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  }

  cyw43_arch_enable_sta_mode();

  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);

  vTaskStartScheduler();
}