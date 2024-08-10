#ifndef _DD_LIS2MDL_H
#define _DD_LIS2MDL_H

#include "magnetometer.h"

bool lis2_init();
bool check_id();
uint32_t get_config_fail_count();
struct MagXYZ get_xyz_uT();

#endif