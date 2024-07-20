#ifndef _DD_MAGNETOMETER_H
#define _DD_MAGNETOMETER_H

struct magXYZ {
  float x_uT;
  float y_uT;
  float z_uT;
};

float getHeading(struct magXYZ *mag);
void vMagnetometerTask(void *pvParameters);

#endif