#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "config.h"
#include "hardware/watchdog.h"
#include "task.h"

void vWatchDogTask() {
  for (;;) {
    watchdog_update();
    printf("Pet Watchdog\n");
    vTaskDelay(WATCHDOG_DELAY_MS);
  }
}

void enable_watchdog() {
  watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
  printf("Watchdog enabled: %ums\n", WATCHDOG_TIMEOUT_MS);
  watchdog_hw->scratch[5] = 0;
  watchdog_hw->scratch[6] = 0;
}