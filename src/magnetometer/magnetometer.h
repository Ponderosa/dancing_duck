#ifndef _DD_MAGNETOMETER_H
#define _DD_MAGNETOMETER_H

#include "semphr.h"

struct MagnetometerTaskParameters {
  QueueHandle_t mag_mailbox;
  SemaphoreHandle_t calibrate;
};

struct MagXYZ {
  double x_uT;
  double y_uT;
  double z_uT;
};

struct CircleCenter {
  double center_x;
  double center_y;
  double rmse;
};

void get_kasa_raw(struct CircleCenter *cr_out);
double get_heading(const struct MagXYZ *mag);
void apply_calibration_kasa(struct MagXYZ *mag);
void vMagnetometerTask(void *pvParameters);
uint32_t get_mag_mailbox_set_error_count();

#endif