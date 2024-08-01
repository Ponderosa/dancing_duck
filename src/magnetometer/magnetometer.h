#ifndef _DD_MAGNETOMETER_H
#define _DD_MAGNETOMETER_H

struct MagXYZ {
  double x_uT;
  double y_uT;
  double z_uT;
};

double get_heading(const struct MagXYZ *mag);
void vMagnetometerTask(void *pvParameters);

#endif