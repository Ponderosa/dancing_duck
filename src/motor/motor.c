#include "FreeRTOS.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "task.h"

#define MOTOR_L_PWM_GPIO 2
#define MOTOR_R_PWM_GPIO 3
#define MOTOR_N_SLEEP_GPIO 4

void vMotorTask() {
  // Tell GPIO 2 and 3 they are allocated to the PWM
  gpio_set_function(MOTOR_L_PWM_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_R_PWM_GPIO, GPIO_FUNC_PWM);

  // Find out which PWM slice is connected to GPIO 0 (it's slice 0)
  uint slice_num = pwm_gpio_to_slice_num(MOTOR_L_PWM_GPIO);

  uint16_t wrap = 999;
  uint16_t level_a = 750;
  uint16_t level_b = 750;
  float clk_div = 4.0;

  // Set period of 1000 cycles (0 to 999 inclusive)
  pwm_set_wrap(slice_num, wrap);
  // Set channel A output high for half the cycles before dropping
  pwm_set_chan_level(slice_num, PWM_CHAN_A, level_a);
  // Set initial B output high for three quarter cycles before dropping
  pwm_set_chan_level(slice_num, PWM_CHAN_B, level_b);
  // Set clk div
  pwm_set_clkdiv(slice_num, clk_div);
  // Set the PWM running
  pwm_set_enabled(slice_num, true);

  gpio_init(MOTOR_N_SLEEP_GPIO);
  gpio_set_dir(MOTOR_N_SLEEP_GPIO, GPIO_OUT);
  gpio_put(MOTOR_N_SLEEP_GPIO, 1);

  bool climb = true;
  int add = 0;

  // Motor Notes:
  // >70% duty cycle to turn on
  // Turns off <65% or so
  // So usable range is between 70% and 100% Duty cycle

  for (;;) {
    // if (level_a >= wrap) {
    //   level_a = 0;
    // } else {
    //   level_a++;
    // }
    // if (level_b >= wrap) {
    //   level_b = 0;
    // } else {
    //   level_b++;
    // }

    // if (level_a <= 0) {
    //   level_a = wrap;
    // } else {
    //   level_a--;
    // }
    // if (level_b <= 0) {
    //   level_b = wrap;
    // } else {
    //   level_b--;
    // }

    // // Set channel A output high for half the cycles before dropping
    // pwm_set_chan_level(slice_num, PWM_CHAN_A, level_a);
    // // Set initial B output high for three quarter cycles before dropping
    // pwm_set_chan_level(slice_num, PWM_CHAN_B, level_b);

    vTaskDelay(30);
  }
}