#ifndef _DD_MAGNETOMETER_H
#define _DD_MAGNETOMETER_H

typedef struct mag_xyz {
  float x_uT;
  float y_uT;
  float z_uT;
} MagXYZ;

void vMagnetometerTask(void *pvParameters);

#endif