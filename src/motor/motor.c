#include "FreeRTOS.h"
#include "commanding.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "task.h"

#define MOTOR_L_PWM_GPIO 2
#define MOTOR_R_PWM_GPIO 3
#define MOTOR_N_SLEEP_GPIO 4

void vMotorTask(void *pvParameters) {
  // Tell GPIO 2 and 3 they are allocated to the PWM
  gpio_set_function(MOTOR_L_PWM_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_R_PWM_GPIO, GPIO_FUNC_PWM);

  // Find out which PWM slice is connected to GPIO 0 (it's slice 0)
  uint slice_num = pwm_gpio_to_slice_num(MOTOR_L_PWM_GPIO);

  uint16_t wrap = 999;
  float clk_div = 4.0;

  pwm_set_wrap(slice_num, wrap);
  pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
  pwm_set_clkdiv(slice_num, clk_div);
  pwm_set_enabled(slice_num, true);

  gpio_init(MOTOR_N_SLEEP_GPIO);
  gpio_set_dir(MOTOR_N_SLEEP_GPIO, GPIO_OUT);
  gpio_put(MOTOR_N_SLEEP_GPIO, 1);

  // Motor Notes:
  // >70% duty cycle to turn on
  // Turns off <65% or so
  // So usable range is between 70% and 100% Duty cycle

  QueueHandle_t *queue = (QueueHandle_t *)pvParameters;
  motorCommand_t mc = {0};

  for (;;) {
    if (xQueueReceive(*queue, &mc, 0)) {
      printf("Setting PWM A to: %u. Setting PWM B to %u.\n", mc.motor_1_duty_cycle,
             mc.motor_2_duty_cycle);
      pwm_set_chan_level(slice_num, PWM_CHAN_A, mc.motor_1_duty_cycle);
      pwm_set_chan_level(slice_num, PWM_CHAN_B, mc.motor_2_duty_cycle);
    }

    vTaskDelay(1000);
  }
}