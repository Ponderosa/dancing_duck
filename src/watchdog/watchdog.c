#include <inttypes.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "config.h"
#include "hardware/watchdog.h"
#include "task.h"

static const uint32_t TOGGLE_PIN_1 = 14;
static const uint32_t TOGGLE_PIN_2 = 15;

void vWatchDogTask() {
  for (;;) {
    watchdog_update();
    if (DEBUG_IDLE) {
      bool is_set = gpio_get(TOGGLE_PIN_1);
      gpio_put(TOGGLE_PIN_1, !is_set);
    } else {
      printf("Pet Watchdog\n");
      vTaskDelay(WATCHDOG_DELAY_MS);
    }
  }
}

void vToggleTask() {
  for (;;) {
    bool is_set = gpio_get(TOGGLE_PIN_2);
    gpio_put(TOGGLE_PIN_2, !is_set);
  }
}

void enable_watchdog() {
  watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
  printf("Watchdog enabled: %" PRIu32 "ms\n", WATCHDOG_TIMEOUT_MS);
  watchdog_hw->scratch[5] = 0;
  watchdog_hw->scratch[6] = 0;

  if (DEBUG_IDLE) {
    gpio_init(TOGGLE_PIN_1);
    gpio_set_dir(TOGGLE_PIN_1, GPIO_OUT);
    gpio_put(TOGGLE_PIN_1, 0);

    gpio_init(TOGGLE_PIN_2);
    gpio_set_dir(TOGGLE_PIN_2, GPIO_OUT);
    gpio_put(TOGGLE_PIN_2, 0);
  }
}