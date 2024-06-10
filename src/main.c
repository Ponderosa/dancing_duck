#include "FreeRTOS.h"
#include "blink.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"
#include "wifi.h"

void vInit() {
  // WiFi chip init
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  } else {
    printf("Wi-Fi init passed!\n");
  }

  cyw43_arch_enable_sta_mode();

  // FreeRTOS Task Creation
  // All task creation should remain here so we can easily manipulate
  // stack size and priority in relationship to one another.
  // Lower number priority is lower priority!
  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);

  // Delete the current task
  vTaskDelete(NULL);
}

void main() {
  stdio_init_all();

  printf("Dancing Duck\n");

  xTaskCreate(vInit, "Init Task", 2048, NULL, 1, NULL);

  vTaskStartScheduler();
}