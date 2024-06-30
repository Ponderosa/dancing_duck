#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "config.h"
#include "task.h"

void vBlinkTask() {
  for (;;) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    if (BLINK_DEBUG) {
      printf("Blink On\n");
    }

    vTaskDelay(BLINK_DELAY_MS);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    if (BLINK_DEBUG) {
      printf("Blink Off\n");
    }

    vTaskDelay(BLINK_DELAY_MS);
  }
}