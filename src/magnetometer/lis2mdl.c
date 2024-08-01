#include <inttypes.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "hardware/i2c.h"
#include "lis2mdl.h"
#include "task.h"

static const bool PRINT_DEBUG = true;

static const uint8_t CFG_REG_A_COMP_TEMP_EN = 0x80;
static const uint8_t CFG_REG_A_ODR_10_HZ = 0x00;
static const uint8_t CFG_REG_A_ODR_20_HZ = 0x04;
static const uint8_t CFG_REG_A_ODR_50_HZ = 0x08;
static const uint8_t CFG_REG_A_ODR_100_HZ = 0x0B;
static const uint8_t CFG_REG_A_SOFT_RESET = 0x20;
static const uint8_t CFG_REG_A_REBOOT = 0x40;

static const uint8_t CFG_REG_B_LPF = 0x01;
static const uint8_t CFG_REG_B_OFFSET_CANCELLATION = 0x02;

static const uint8_t CFG_REG_C_BDU = 0x10;

static const uint32_t I2C_BAUD = 1000 * 1000;
static const uint32_t I2C_TIMEOUT_US = 1000;
static const uint8_t I2C_ADDRESS = 0x1E;
static const double GAUSS_RANGE = 49.152;
static const double ONE_GAUSS_IN_UT = 100.0;

static const uint8_t WHO_AM_I_ADDRESS = 0x4F;
static const uint8_t WHO_AM_I_ID = 0x40;
static const uint8_t CONFIG_REGS_WRITE[4] = {0x60, (CFG_REG_A_COMP_TEMP_EN | CFG_REG_A_ODR_50_HZ),
                                             (CFG_REG_B_LPF), (CFG_REG_C_BDU)};
static const uint8_t CONFIG_SOFT_RESET[2] = {0x60, (CFG_REG_A_SOFT_RESET)};
static const uint8_t CONFIG_REBOOT[2] = {0x60, (CFG_REG_A_REBOOT)};

static const uint8_t OUT_ADDRESS = 0x68;

// Magnetometer hard iron calibration data
static const double MAG_OFFSET_DUCK_NUM_INDEX_X_UT[20] = {0.0, -29.20, 10.7, 0.0, -12.5, 0.0};
static const double MAG_OFFSET_DUCK_NUM_INDEX_Y_UT[20] = {0.0, -49.78, 2.3, 0.0, -20.7, 0.0};
static const double factor = GAUSS_RANGE * ONE_GAUSS_IN_UT / INT16_MAX;

static double binary_to_ut(int16_t in) { return (double)in * factor; }

static int16_t ut_to_binary(double in) { return (int16_t)(in / factor); }

static void set_hard_iron() {
  uint8_t offset[5] = {0x45, 0x00, 0x00, 0x00, 0x00};
  int16_t x_off = ut_to_binary(MAG_OFFSET_DUCK_NUM_INDEX_X_UT[DUCK_ID_NUM]);
  offset[1] = (uint8_t)(x_off & 0xFF);
  offset[2] = (uint8_t)((x_off >> 8) & 0xFF);
  int16_t y_off = ut_to_binary(MAG_OFFSET_DUCK_NUM_INDEX_Y_UT[DUCK_ID_NUM]);
  offset[3] = (uint8_t)(y_off & 0xFF);
  offset[4] = (uint8_t)((y_off >> 8) & 0xFF);

  // printf("INT16 IN: %04X:%04x\n", x_off, y_off);
  // printf("HIR IN: %02X:%02X:%02X:%02X\n", offset[1], offset[2], offset[3], offset[4]);

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, offset, sizeof(offset), true, I2C_TIMEOUT_US);
}

static void print_hard_iron() {
  uint8_t in_buffer[6] = {0};
  uint8_t offset = 0x45;

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &offset, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, in_buffer, 6, false, I2C_TIMEOUT_US);

  printf("HIR OUT: %02X:%02X:%02X:%02X\n", in_buffer[0], in_buffer[1], in_buffer[2], in_buffer[3]);

  // X
  int16_t mag = in_buffer[0] | (in_buffer[1] << 8);
  double uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL HI X: %04x\n", mag);
    printf("LIS2MDL uT X: %f\n", uT);
  }

  // Y
  mag = in_buffer[2] | (in_buffer[3] << 8);
  uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL HI Y: %04x\n", mag);
    printf("LIS2MDL uT Y: %f\n", uT);
  }

  // Z
  mag = in_buffer[4] | (in_buffer[5] << 8);
  uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL HI Z: %04x\n", mag);
    printf("LIS2MDL uT Z: %f\n", uT);
  }
}

static void lis2_reboot() {
  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, CONFIG_SOFT_RESET, sizeof(CONFIG_SOFT_RESET), true,
                       I2C_TIMEOUT_US);
  sleep_ms(1);
  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, CONFIG_REBOOT, sizeof(CONFIG_REBOOT), true,
                       I2C_TIMEOUT_US);
  sleep_ms(25);
}

bool lis2_init() {
  // Physical pullups on LIS2MDL daughter board
  gpio_set_function(20, GPIO_FUNC_I2C);
  gpio_set_function(21, GPIO_FUNC_I2C);
  i2c_init(&i2c0_inst, I2C_BAUD);
  lis2_reboot();
  // print_hard_iron();
  set_hard_iron();
  // print_hard_iron();
  //    Enable internal continous sensor reading at 10hz
  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, CONFIG_REGS_WRITE, sizeof(CONFIG_REGS_WRITE), true,
                       I2C_TIMEOUT_US);
  return check_id();
}

bool check_id() {
  uint8_t in_buffer[1] = {0};

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &WHO_AM_I_ADDRESS, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 1, false, I2C_TIMEOUT_US);
  printf("LIS2MDL ID: 0x%02hhX\n", in_buffer[0]);
  return WHO_AM_I_ID == in_buffer[0];
}

struct MagXYZ get_xyz_uT() {
  uint8_t in_buffer[16] = {0};

  struct MagXYZ ret_val = {0.0, 0.0, 0.0};

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &OUT_ADDRESS, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 8, false, I2C_TIMEOUT_US);

  // X
  int16_t mag = in_buffer[0] | (in_buffer[1] << 8);
  ret_val.x_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    // printf("LIS2MDL OUT X: %" PRIi16 "\n", mag);
  }

  // Y
  mag = in_buffer[2] | (in_buffer[3] << 8);
  ret_val.y_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    // printf("LIS2MDL OUT Y: %" PRIi16 "\n", mag);
  }

  // Z
  mag = in_buffer[4] | (in_buffer[5] << 8);
  ret_val.z_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    // printf("LIS2MDL OUT Z: %" PRIi16 "\n", mag);
  }

  return ret_val;
}