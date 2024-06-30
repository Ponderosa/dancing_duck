#include "FreeRTOS.h"

#include "pico/stdlib.h"

#include "config.h"
#include "hardware/watchdog.h"
#include "task.h"

void vWatchDogTask() {
  for (;;) {
    watchdog_update();

    vTaskDelay(WATCHDOG_DELAY_MS);
  }
}