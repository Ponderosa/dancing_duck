#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "hardware/i2c.h"
#include "lis2mdl.h"
#include "task.h"

#define I2C_BAUD        (1000 * 1000)
#define I2C_TIMEOUT_US  (1000)
#define I2C_ADDRESS     (0x1E)
#define GAUSS_RANGE     (50.0)
#define ONE_GAUSS_IN_UT (100.0)

static const uint8_t WHO_AM_I_ADDRESS = 0x4F;
static const uint8_t WHO_AM_I_ID = 0x40;
static const uint8_t CONFIG_CONTINUOUS[2] = {0x60, 0x00};
static const uint8_t OUT_ADDRESS = 0x67;

static float binary_to_ut(int16_t in) {
  static const float factor = GAUSS_RANGE * ONE_GAUSS_IN_UT / INT16_MAX;
  return (float)in * factor;
}

bool init() {
  // Physical pullups on LIS2MDL daughter board
  gpio_set_function(20, GPIO_FUNC_I2C);
  gpio_set_function(21, GPIO_FUNC_I2C);
  i2c_init(&i2c0_inst, I2C_BAUD);
  // Enable internal continous sensor reading at 10hz
  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &CONFIG_CONTINUOUS[0], 2, true, I2C_TIMEOUT_US);
  return check_id();
}

bool check_id() {
  uint8_t in_buffer[1] = {0};

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &WHO_AM_I_ADDRESS, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 1, false, I2C_TIMEOUT_US);
  printf("LIS2MDL ID: 0x%02hhX\n", in_buffer[0]);
  return WHO_AM_I_ID == in_buffer[0];
}

MagXYZ get_xyz_uT() {
  uint8_t in_buffer[16] = {0};

  MagXYZ ret_val = {0.0, 0.0, 0.0};

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &OUT_ADDRESS, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 9, false, I2C_TIMEOUT_US);
  printf("LIS2MDL Status: 0x%02hhX\n", in_buffer[0]);
  int16_t mag = in_buffer[1] | (in_buffer[2] << 8);
  ret_val.x_uT = binary_to_ut(mag);
  printf("LIS2MDL OUT X: %d\n", mag);
  mag = in_buffer[3] | (in_buffer[4] << 8);
  ret_val.y_uT = binary_to_ut(mag);
  printf("LIS2MDL OUT Y: %d\n", mag);
  mag = in_buffer[5] | (in_buffer[6] << 8);
  ret_val.z_uT = binary_to_ut(mag);
  printf("LIS2MDL OUT Z: %d\n", mag);

  return ret_val;
}