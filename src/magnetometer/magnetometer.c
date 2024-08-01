#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "lis2mdl.h"
#include "math.h"
#include "queue.h"
#include "task.h"

double get_heading(const struct MagXYZ *mag) {
  // Invert X reading due to placement of sensor
  double heading = atan2l((double)mag->y_uT, -(double)mag->x_uT);

  heading *= 180.0 / (double)M_PI;

  // Normalize to 0-360 degrees
  if (heading < 0) {
    heading += 360.0;
  }

  return heading;
}

void vMagnetometerTask(void *pvParameters) {
  QueueHandle_t mailbox = (QueueHandle_t)pvParameters;
  if (lis2_init() == false) {
    printf("Magnetometer Init Failed!\n");
  }
  for (;;) {
    struct MagXYZ mag = get_xyz_uT();
    xQueueOverwrite(mailbox, &mag);
    vTaskDelay(100);
  }
}