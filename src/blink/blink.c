#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"

void vBlinkTask() {
  for (;;) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    // printf("Blink On\n");

    vTaskDelay(500);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    // printf("Blink Off\n");

    vTaskDelay(500);
  }
}