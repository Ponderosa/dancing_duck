#include "FreeRTOS.h"

#include "pico/stdlib.h"

#include "commanding.h"
#include "hardware/pwm.h"
#include "task.h"

// See datasheet section 4.5.2 to ensure chosen GPIO are paired to same slice
#define MOTOR_L_PWM_GPIO     2
#define MOTOR_R_PWM_GPIO     3
#define MOTOR_N_SLEEP_GPIO   4

#define COUNTER_WRAP_COUNT   999
#define COUNTER_CLK_DIV      (4.0f)
#define MAX_DELAY_S          (UINT32_MAX / 1000)
#define BUFFER_EMPTY_DELAY_S 3

void vMotorTask(void *pvParameters) {
  gpio_set_function(MOTOR_L_PWM_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_R_PWM_GPIO, GPIO_FUNC_PWM);

  uint slice_num = pwm_gpio_to_slice_num(MOTOR_L_PWM_GPIO);

  pwm_set_wrap(slice_num, COUNTER_WRAP_COUNT);
  pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
  pwm_set_clkdiv(slice_num, COUNTER_CLK_DIV);
  pwm_set_enabled(slice_num, true);

  gpio_init(MOTOR_N_SLEEP_GPIO);
  gpio_set_dir(MOTOR_N_SLEEP_GPIO, GPIO_OUT);
  gpio_put(MOTOR_N_SLEEP_GPIO, 0);

  // Motor Notes:
  // >70% duty cycle to turn on
  // Turns off <65% or so
  // So usable range is between 70% and 100% Duty cycle

  QueueHandle_t *queue = (QueueHandle_t *)pvParameters;
  motorCommand_t mc = {0};

  for (;;) {
    int duration_factor = 1;
    bool motor_driver_sleep = true;

    if (xQueueReceive(*queue, &mc, 0)) {
      if (mc.motor_1_duty_cycle > COUNTER_WRAP_COUNT ||
          mc.motor_2_duty_cycle > COUNTER_WRAP_COUNT || mc.duration_s > MAX_DELAY_S) {
        printf("Error: PWM Settings out of range!\n");
      } else {
        duration_factor = mc.duration_s;
        pwm_set_chan_level(slice_num, PWM_CHAN_A, mc.motor_1_duty_cycle);
        pwm_set_chan_level(slice_num, PWM_CHAN_B, mc.motor_2_duty_cycle);
        printf("Set PWM A to: %u. Set PWM B to %u. For %lu seconds.\n", mc.motor_1_duty_cycle,
               mc.motor_2_duty_cycle, duration_factor);
        motor_driver_sleep = false;
      }
    } else {
      printf("Motor Command Buffer Empty!\n");
      duration_factor = BUFFER_EMPTY_DELAY_S;
      pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
      pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
      printf("Set PWM A to: %u. Set PWM B to %u. For %lu seconds.\n", 0, 0, duration_factor);
    }

    gpio_put(MOTOR_N_SLEEP_GPIO, motor_driver_sleep ? 0 : 1);

    vTaskDelay(duration_factor * 1000);
  }
}