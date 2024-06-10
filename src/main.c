#include "FreeRTOS.h"
#include "blink.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"
#include "wifi.h"

#define WIFI_SCAN (1)

void vInit() {
  // WiFi chip init - Must be ran in FreeRTOS Task
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  } else {
    printf("Wi-Fi init passed!\n");
  }

  cyw43_arch_enable_sta_mode();

  if (!WIFI_SCAN) {
    printf("Connecting to Wi-Fi...\n");
    printf("WiFi SSID: %s\n", WIFI_SSID);
    printf("WiFI Password: %s\n", WIFI_PASSWORD);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
      printf("failed to connect.\n");
    } else {
      printf("Connected.\n");
    }
  }

  // FreeRTOS Task Creation
  // All task creation should remain here so we can easily manipulate
  // stack size and priority in relationship to one another.
  // Lower number priority is lower priority!
  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  if (WIFI_SCAN) {
    xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);
  } else {
    xTaskCreate(vPing, "Ping Task", 2048, NULL, 2, NULL);
  }

  // Delete the current task
  vTaskDelete(NULL);
}

void main() {
  stdio_init_all();

  printf("Dancing Duck\n");

  xTaskCreate(vInit, "Init Task", 2048, NULL, 1, NULL);

  vTaskStartScheduler();
}