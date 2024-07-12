#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "commanding.h"
#include "hardware/pwm.h"
#include "task.h"

#define DEBUG_PRINTF                   0

// The shape of the props are inverted to one another
#define MIRRORED_PROPS                 1

// See datasheet section 4.5.2 to ensure chosen GPIO are paired to same slice
#define MOTOR_A_RIGHT_FORWARD_PWM_GPIO 2
#define MOTOR_A_RIGHT_REVERSE_PWM_GPIO 3
#define MOTOR_B_LEFT_FORWARD_PWM_GPIO  4
#define MOTOR_B_LEFT_REVERSE_PWM_GPIO  5
#define MOTOR_N_SLEEP_GPIO             6
#define MOTOR_FAULT_GPIO               7

#define COUNTER_WRAP_COUNT             999
#define COUNTER_CLK_DIV                (4.0f)
#define BUFFER_EMPTY_DELAY_MS          3000

int bi_unit_clamp_and_expand(float val) {
  float ret_val = val;
  if (val > 1.0) {
    ret_val = 1.0;
  }
  if (val < -1.0) {
    ret_val = -1.0;
  }

  return (int)(ret_val * (float)COUNTER_WRAP_COUNT);
}

void vMotorTask(void *pvParameters) {
  gpio_set_function(MOTOR_A_RIGHT_FORWARD_PWM_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_A_RIGHT_REVERSE_PWM_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_B_LEFT_FORWARD_PWM_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_B_LEFT_REVERSE_PWM_GPIO, GPIO_FUNC_PWM);

  uint slice_num_a_right = pwm_gpio_to_slice_num(MOTOR_A_RIGHT_FORWARD_PWM_GPIO);

  pwm_set_wrap(slice_num_a_right, COUNTER_WRAP_COUNT);
  pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, 0);
  pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, 0);
  pwm_set_clkdiv(slice_num_a_right, COUNTER_CLK_DIV);
  pwm_set_enabled(slice_num_a_right, true);

  uint slice_num_b_left = pwm_gpio_to_slice_num(MOTOR_B_LEFT_FORWARD_PWM_GPIO);

  pwm_set_wrap(slice_num_b_left, COUNTER_WRAP_COUNT);
  pwm_set_chan_level(slice_num_b_left, PWM_CHAN_A, 0);
  pwm_set_chan_level(slice_num_b_left, PWM_CHAN_B, 0);
  pwm_set_clkdiv(slice_num_b_left, COUNTER_CLK_DIV);
  pwm_set_enabled(slice_num_b_left, true);

  gpio_init(MOTOR_N_SLEEP_GPIO);
  gpio_set_dir(MOTOR_N_SLEEP_GPIO, GPIO_OUT);
  gpio_put(MOTOR_N_SLEEP_GPIO, 0);

  gpio_init(MOTOR_FAULT_GPIO);
  gpio_set_dir(MOTOR_FAULT_GPIO, GPIO_IN);

  // Motor Notes:
  // >70% duty cycle to turn on
  // Turns off <65% or so
  // So usable range is between 70% and 100% Duty cycle

  QueueHandle_t *queue = (QueueHandle_t *)pvParameters;
  motorCommand_t mc = {0};

  for (;;) {
    int delay;
    bool motor_driver_sleep = true;

    if (xQueueReceive(*queue, &mc, 0)) {
      if (gpio_get(MOTOR_FAULT_GPIO)) {
        printf("DRV Fault!\n");
      }

      int motor_right_duty = bi_unit_clamp_and_expand(mc.motor_right_duty_cycle);
      int motor_left_duty = bi_unit_clamp_and_expand(mc.motor_left_duty_cycle);

      delay = mc.duration_ms;
      // Motor 1
      if (motor_right_duty > 0) {
        pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, motor_right_duty);
        pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, 0);
      } else {
        pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, 0);
        pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, -motor_right_duty);
      }
      // Motor 2
      if (MIRRORED_PROPS) {
        motor_left_duty *= -1.0;
      }
      if (motor_left_duty > 0) {
        pwm_set_chan_level(slice_num_b_left, PWM_CHAN_A, motor_left_duty);
        pwm_set_chan_level(slice_num_b_left, PWM_CHAN_B, 0);
      } else {
        pwm_set_chan_level(slice_num_b_left, PWM_CHAN_A, 0);
        pwm_set_chan_level(slice_num_b_left, PWM_CHAN_B, -motor_left_duty);
      }

      if (motor_right_duty || motor_left_duty) {
        motor_driver_sleep = false;
      }

      if (DEBUG_PRINTF) {
        printf("Right Duty: %d, Left Duty: %d, For %d ms.\n", motor_right_duty, motor_left_duty,
               delay);
      }
    } else {
      if (DEBUG_PRINTF) {
        printf("Motor Command Buffer Empty!\n");
      }

      delay = BUFFER_EMPTY_DELAY_MS;
      pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, 0);
      pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, 0);
      pwm_set_chan_level(slice_num_b_left, PWM_CHAN_A, 0);
      pwm_set_chan_level(slice_num_b_left, PWM_CHAN_B, 0);

      if (DEBUG_PRINTF) {
        printf("Set PWM A to: %u. Set PWM B to %u. For %d ms.\n", 0, 0, delay);
      }
    }

    gpio_put(MOTOR_N_SLEEP_GPIO, motor_driver_sleep ? 0 : 1);

    vTaskDelay(delay);
  }
}