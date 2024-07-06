#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "lis2mdl.h"
#include "queue.h"
#include "task.h"

void vMagnetometerTask(void *pvParameters) {
  QueueHandle_t *mailbox = (QueueHandle_t *)pvParameters;
  init();
  for (;;) {
    MagXYZ mag = get_xyz_uT();
    xQueueOverwrite(*mailbox, &mag);
    vTaskDelay(100);
  }
}