#include "FreeRTOS.h"
#include "blink.h"
#include "pico/async_context_freertos.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"
#include "wifi.h"

void main() {
  stdio_init_all();

  printf("Dancing Duck\n");

  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);

  vTaskStartScheduler();
}