#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "hardware/i2c.h"
#include "lis2mdl.h"
#include "task.h"

#define PRINT_DEBUG     0

#define I2C_BAUD        (1000 * 1000)
#define I2C_TIMEOUT_US  (1000)
#define I2C_ADDRESS     (0x1E)
#define GAUSS_RANGE     (50.0)
#define ONE_GAUSS_IN_UT (100.0)

static const uint8_t WHO_AM_I_ADDRESS = 0x4F;
static const uint8_t WHO_AM_I_ID = 0x40;
static const uint8_t CONFIG_CONTINUOUS[2] = {0x60, 0x00};
static const uint8_t OUT_ADDRESS = 0x68;

// Magnetometer hard iron calibration data
static const float MAG_OFFSET_DUCK_NUM_INDEX_X_UT[20] = {6.41, -21.90, 15.93};
static const float MAG_OFFSET_DUCK_NUM_INDEX_Y_UT[20] = {-40.20, -29.18, -19.56};
static const float factor = GAUSS_RANGE * ONE_GAUSS_IN_UT / INT16_MAX;

static float binary_to_ut(int16_t in) { return (float)in * factor; }

static int16_t ut_to_binary(float in) { return (int16_t)(in / factor); }

static void set_hard_iron() {
  uint8_t offset[5] = {0x45, 0x00, 0x00, 0x00, 0x00};
  int16_t x_off = ut_to_binary(MAG_OFFSET_DUCK_NUM_INDEX_X_UT[DUCK_ID_NUM]);
  offset[1] = (uint8_t)(x_off & 0xFF);
  offset[2] = (uint8_t)((x_off >> 8) & 0xFF);
  int16_t y_off = ut_to_binary(MAG_OFFSET_DUCK_NUM_INDEX_Y_UT[DUCK_ID_NUM]);
  offset[3] = (uint8_t)(y_off & 0xFF);
  offset[4] = (uint8_t)((y_off >> 8) & 0xFF);

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &offset[0], 5, true, I2C_TIMEOUT_US);
}

bool init() {
  // Physical pullups on LIS2MDL daughter board
  gpio_set_function(20, GPIO_FUNC_I2C);
  gpio_set_function(21, GPIO_FUNC_I2C);
  i2c_init(&i2c0_inst, I2C_BAUD);
  set_hard_iron();
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
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 8, false, I2C_TIMEOUT_US);

  // X
  int16_t mag = in_buffer[0] | (in_buffer[1] << 8);
  ret_val.x_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL OUT X: %d\n", mag);
  }

  // Y
  mag = in_buffer[2] | (in_buffer[3] << 8);
  ret_val.y_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL OUT Y: %d\n", mag);
  }

  // Z
  mag = in_buffer[4] | (in_buffer[5] << 8);
  ret_val.z_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL OUT Z: %d\n", mag);
  }

  return ret_val;
}