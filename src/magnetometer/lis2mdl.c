#include <inttypes.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "hardware/i2c.h"
#include "lis2mdl.h"
#include "task.h"

static const bool PRINT_DEBUG = false;

#define I2C_ADDRESS                   0x1E

#define CFG_REG_A_COMP_TEMP_EN        0x80
#define CFG_REG_A_ODR_10_HZ           0x00
#define CFG_REG_A_ODR_20_HZ           0x04
#define CFG_REG_A_ODR_50_HZ           0x08
#define CFG_REG_A_ODR_100_HZ          0x0B
#define CFG_REG_A_SOFT_RESET          0x20
#define CFG_REG_A_REBOOT              0x40

#define CFG_REG_B_LPF                 0x01
#define CFG_REG_B_OFFSET_CANCELLATION 0x02

#define CFG_REG_C_BDU                 0x10

#define CONFIG_ADDRESS                0x60
#define OUT_ADDRESS                   0x68
#define HARD_IRON_ADDRESS             0x45
#define WHO_AM_I_ADDRESS              0x4F
#define WHO_AM_I_ID                   0x40

static const uint8_t CONFIG_REGS_WRITE[4] = {CONFIG_ADDRESS,
                                             (CFG_REG_A_COMP_TEMP_EN | CFG_REG_A_ODR_50_HZ),
                                             (CFG_REG_B_LPF), (CFG_REG_C_BDU)};
static const uint8_t CONFIG_SOFT_RESET[2] = {CONFIG_ADDRESS, (CFG_REG_A_SOFT_RESET)};
static const uint8_t CONFIG_REBOOT[2] = {CONFIG_ADDRESS, (CFG_REG_A_REBOOT)};

static const uint32_t I2C_BAUD = 1000 * 1000;
static const uint32_t I2C_TIMEOUT_US = 1000;

static const double GAUSS_RANGE = 49.152;
static const double ONE_GAUSS_IN_UT = 100.0;

static const double factor = GAUSS_RANGE * ONE_GAUSS_IN_UT / INT16_MAX;

static double binary_to_ut(int16_t in) { return (double)in * factor; }

static void lis2_reboot() {
  // From AN5069
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
  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, CONFIG_REGS_WRITE, sizeof(CONFIG_REGS_WRITE), true,
                       I2C_TIMEOUT_US);
  return check_id();
}

bool check_id() {
  uint8_t in_buffer[1] = {0};
  uint8_t read_address = WHO_AM_I_ADDRESS;

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &read_address, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 1, false, I2C_TIMEOUT_US);
  printf("LIS2MDL ID: 0x%02hhX\n", in_buffer[0]);
  return WHO_AM_I_ID == in_buffer[0];
}

struct MagXYZ get_xyz_uT() {
  uint8_t in_buffer[16] = {0};
  uint8_t read_address = OUT_ADDRESS;

  struct MagXYZ ret_val = {0.0, 0.0, 0.0};

  i2c_write_timeout_us(&i2c0_inst, I2C_ADDRESS, &read_address, 1, true, I2C_TIMEOUT_US);
  i2c_read_timeout_us(&i2c0_inst, I2C_ADDRESS, &in_buffer[0], 6, false, I2C_TIMEOUT_US);

  // X
  int16_t mag = in_buffer[0] | (in_buffer[1] << 8);
  ret_val.x_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL OUT X: %" PRIi16 "\n", mag);
  }

  // Y
  mag = in_buffer[2] | (in_buffer[3] << 8);
  ret_val.y_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL OUT Y: %" PRIi16 "\n", mag);
  }

  // Z
  mag = in_buffer[4] | (in_buffer[5] << 8);
  ret_val.z_uT = binary_to_ut(mag);
  if (PRINT_DEBUG) {
    printf("LIS2MDL OUT Z: %" PRIi16 "\n", mag);
  }

  return ret_val;
}