#ifndef _DD_LIS2MDL_H
#define _DD_LIS2MDL_H

#include "magnetometer.h"

bool init();
bool check_id();
float get_x_uT();
float get_y_uT();
float get_z_uT();
MagXYZ get_xyz_uT();

#endif