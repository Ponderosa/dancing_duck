#ifndef _DD_MAGNETOMETER_H
#define _DD_MAGNETOMETER_H

struct MagXYZ {
  float x_uT;
  float y_uT;
  float z_uT;
};

float getHeading(struct MagXYZ *mag);
void vMagnetometerTask(void *pvParameters);

#endif