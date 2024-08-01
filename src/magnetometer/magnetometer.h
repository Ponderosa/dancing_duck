#ifndef _DD_MAGNETOMETER_H
#define _DD_MAGNETOMETER_H

struct MagXYZ {
  double x_uT;
  double y_uT;
  double z_uT;
};

struct CircleResult {
  double center_x;
  double center_y;
  double rmse;
};

void get_kasa(struct CircleResult *cr_out);
double get_heading(const struct MagXYZ *mag);
void vMagnetometerTask(void *pvParameters);

#endif