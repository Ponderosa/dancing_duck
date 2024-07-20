#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "commanding.h"
#include "hardware/pwm.h"
#include "magnetometer.h"
#include "math.h"
#include "motor.h"
#include "stdio.h"
#include "string.h"
#include "task.h"

#define DEBUG_PRINTF                      1
#define LOOP_DELAY_MS                     100

// The shape of the props are inverted to one another
#define MIRRORED_PROPS                    1
#define LEFT_INVERTED                     0
#define RIGHT_INVERTED                    1

#define POINT_DEADBAND_PLUS_MINUS_DEGREES 5.0f
#define MIN_DUTY_CYCLE                    0.7f
#define BASE_DUTY_CYCLE                   ((1.0f - MIN_DUTY_CYCLE) / 2.0 + MIN_DUTY_CYCLE)

// See datasheet section 4.5.2 to ensure chosen GPIO are paired to same slice
#define MOTOR_A_RIGHT_FORWARD_PWM_GPIO    2
#define MOTOR_A_RIGHT_REVERSE_PWM_GPIO    3
#define MOTOR_B_LEFT_FORWARD_PWM_GPIO     4
#define MOTOR_B_LEFT_REVERSE_PWM_GPIO     5
#define MOTOR_N_SLEEP_GPIO                6
#define MOTOR_FAULT_GPIO                  7

#define COUNTER_WRAP_COUNT                999
#define COUNTER_CLK_DIV                   (4.0f)
#define BUFFER_EMPTY_DELAY_MS             3000

static void initMotor();
static void setMotor(struct motorCommand *);

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

static void point(struct motorCommand *mc, struct magXYZ *mag) {
  float current_heading = getHeading(mag);
  float angle_diff = mc->desired_heading - current_heading;

  // Normalize the angle difference to be between -180 and 180 degrees
  if (angle_diff > 180.0f) {
    angle_diff -= 360.0f;
  } else if (angle_diff < -180.0f) {
    angle_diff += 360.0f;
  }

  // Determine rotation direction
  if (fabsf(angle_diff) < POINT_DEADBAND_PLUS_MINUS_DEGREES) {
    // No rotation
    mc->motor_left_duty_cycle = 0.0;
    mc->motor_right_duty_cycle = 0.0;
  } else if (angle_diff > 0) {
    // Clockwise
    mc->motor_left_duty_cycle = MIN_DUTY_CYCLE;
    mc->motor_right_duty_cycle = 0.0;
  } else {
    // Counter Clockwise
    mc->motor_left_duty_cycle = 0.0;
    mc->motor_right_duty_cycle = MIN_DUTY_CYCLE;
  }
}

static void swim(struct motorCommand *mc, struct magXYZ *mag) {
  float current_heading = getHeading(mag);
  float error = mc->desired_heading - current_heading;

  // Normalize the angle difference to be between -180 and 180 degrees
  if (error > 180.0f) {
    error -= 360.0f;
  } else if (error < -180.0f) {
    error += 360.0f;
  }

  // PD Controls
  float derivative = error - mc->previous_error;
  float adjustment = mc->Kp * error + mc->Kd * derivative;
  mc->previous_error = error;

  if (DEBUG_PRINTF) {
    printf("error: %f adjustment: %f\n", error, adjustment);
  }

  // Set prop speeds
  mc->motor_left_duty_cycle = BASE_DUTY_CYCLE + adjustment;
  if (mc->motor_left_duty_cycle < 0.0) {
    mc->motor_left_duty_cycle = 0.0;
  }
  mc->motor_right_duty_cycle = BASE_DUTY_CYCLE - adjustment;
  if (mc->motor_right_duty_cycle < 0.0) {
    mc->motor_right_duty_cycle = 0.0;
  }

  if (DEBUG_PRINTF) {
    printf("duty left: %f duty right: %f\n", mc->motor_left_duty_cycle, mc->motor_right_duty_cycle);
  }
}

// Motor Notes:
// >70% duty cycle to turn on
// Turns off <65% or so
// So usable range is between 70% and 100% Duty cycle
void vMotorTask(void *pvParameters) {
  initMotor();

  struct motorQueues *mq = (struct motorQueues *)pvParameters;
  struct motorCommand mc = {0};

  vTaskDelay(1000);

  for (;;) {
    // Load motor command if previous mc expired
    if (!mc.remaining_time_ms) {
      if (xQueueReceive(*(mq->command_queue), &mc, 0)) {
        // Todo: Increase motor cmd executed count metric
        if (DEBUG_PRINTF) {
          printf("Motor msg rx: %ldms\n", mc.remaining_time_ms);
        }
      } else {
        memset(&mc, 0, sizeof(mc));
      }
    }

    // Read current heading
    struct magXYZ mag = {0};
    xQueuePeek(*(mq->mag_queue), &mag, 0);

    // Perform motor algorithm
    if (mc.remaining_time_ms) {
      switch (mc.type) {
        case MOTOR:
          // No manipulation needed
          break;
        case POINT:
          point(&mc, &mag);
          break;
        case SWIM:
          swim(&mc, &mag);
          break;
        default:
          memset(&mc, 0, sizeof(mc));
      }
    }

    // Update PWM and Sleep Pin
    setMotor(&mc);

    // Check Fault Pin
    if (gpio_get(MOTOR_FAULT_GPIO)) {
      printf("DRV Fault!\n");  // Todo: make this a log
    }

    // Check remaining time
    if (mc.remaining_time_ms > LOOP_DELAY_MS) {
      mc.remaining_time_ms -= LOOP_DELAY_MS;
    } else {
      mc.remaining_time_ms = 0;
    }

    vTaskDelay(LOOP_DELAY_MS);
  }
}

static void setMotor(struct motorCommand *mc) {
  int motor_right_duty = bi_unit_clamp_and_expand(mc->motor_right_duty_cycle);
  int motor_left_duty = bi_unit_clamp_and_expand(mc->motor_left_duty_cycle);

  uint slice_num_a_right = pwm_gpio_to_slice_num(MOTOR_A_RIGHT_FORWARD_PWM_GPIO);
  uint slice_num_b_left = pwm_gpio_to_slice_num(MOTOR_B_LEFT_FORWARD_PWM_GPIO);

  bool motor_driver_sleep = true;

  // Motor 1
  if (MIRRORED_PROPS && RIGHT_INVERTED) {
    motor_right_duty *= -1.0;
  }
  if (motor_right_duty > 0) {
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, motor_right_duty);
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, 0);
  } else {
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, 0);
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, -motor_right_duty);
  }
  // Motor 2
  if (MIRRORED_PROPS && LEFT_INVERTED) {
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
    printf("Right Duty: %d, Left Duty: %d\n", motor_right_duty, motor_left_duty);
  }

  gpio_put(MOTOR_N_SLEEP_GPIO, motor_driver_sleep ? 0 : 1);
}

static void initMotor() {
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
}