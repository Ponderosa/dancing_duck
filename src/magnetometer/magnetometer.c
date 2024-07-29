#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "lis2mdl.h"
#include "math.h"
#include "queue.h"
#include "task.h"

float getHeading(struct magXYZ *mag) {
  // Invert X reading due to placement of sensor
  float heading = atan2f((float)mag->y_uT, -(float)mag->x_uT);

  heading *= 180.0f / M_PI;

  // Normalize to 0-360 degrees
  if (heading < 0) {
    heading += 360.0f;
  }

  return heading;
}

void vMagnetometerTask(void *pvParameters) {
  QueueHandle_t mailbox = (QueueHandle_t)pvParameters;
  init();
  for (;;) {
    struct magXYZ mag = get_xyz_uT();
    xQueueOverwrite(mailbox, &mag);
    vTaskDelay(100);
  }
}