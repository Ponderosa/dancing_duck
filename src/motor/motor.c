#include <inttypes.h>

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

static const bool DEBUG_PRINT = false;
static const uint32_t LOOP_DELAY_MS = 100;

// The shape of the props are inverted to one another
static const bool MIRRORED_PROPS = true;
static const bool LEFT_INVERTED = false;
static const bool RIGHT_INVERTED = true;

static const double POINT_DEADBAND_PLUS_MINUS_DEGREES = 10.0;
static const double MIN_DUTY_CYCLE = 0.7;
static const double BASE_DUTY_CYCLE =
    ((1.0 - MIN_DUTY_CYCLE) / 2.0 + MIN_DUTY_CYCLE);  // Find mid point

// See datasheet section 4.5.2 to ensure chosen GPIO are paired to same slice
static const uint32_t MOTOR_A_RIGHT_FORWARD_PWM_GPIO = 2;
static const uint32_t MOTOR_A_RIGHT_REVERSE_PWM_GPIO = 3;
static const uint32_t MOTOR_B_LEFT_FORWARD_PWM_GPIO = 4;
static const uint32_t MOTOR_B_LEFT_REVERSE_PWM_GPIO = 5;
static const uint32_t MOTOR_N_SLEEP_GPIO = 6;
static const uint32_t MOTOR_FAULT_GPIO = 7;

static const uint32_t COUNTER_WRAP_COUNT = 999;
static const double COUNTER_CLK_DIV = 4.0;

static uint32_t motor_cmd_rx_count = 0;

static void init_motor() {
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

static int16_t bi_unit_clamp_and_expand(double val) {
  double ret_val = val;
  if (val > 1.0) {
    ret_val = 1.0;
  }
  if (val < -1.0) {
    ret_val = -1.0;
  }

  return (int16_t)(ret_val * (double)COUNTER_WRAP_COUNT);
}

static void set_motor(struct MotorCommand *mc) {
  int16_t motor_right_duty = bi_unit_clamp_and_expand(mc->motor_right_duty_cycle);
  int16_t motor_left_duty = bi_unit_clamp_and_expand(mc->motor_left_duty_cycle);

  uint slice_num_a_right = pwm_gpio_to_slice_num(MOTOR_A_RIGHT_FORWARD_PWM_GPIO);
  uint slice_num_b_left = pwm_gpio_to_slice_num(MOTOR_B_LEFT_FORWARD_PWM_GPIO);

  bool motor_driver_sleep = true;

  // Motor 1
  if (MIRRORED_PROPS && RIGHT_INVERTED) {
    motor_right_duty = -motor_right_duty;
  }
  if (motor_right_duty > 0) {
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, (uint16_t)motor_right_duty);
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, 0);
  } else {
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_A, 0);
    pwm_set_chan_level(slice_num_a_right, PWM_CHAN_B, (uint16_t)-motor_right_duty);
  }
  // Motor 2
  if (MIRRORED_PROPS && LEFT_INVERTED) {
    motor_left_duty = -motor_left_duty;
  }
  if (motor_left_duty > 0) {
    pwm_set_chan_level(slice_num_b_left, PWM_CHAN_A, (uint16_t)motor_left_duty);
    pwm_set_chan_level(slice_num_b_left, PWM_CHAN_B, 0);
  } else {
    pwm_set_chan_level(slice_num_b_left, PWM_CHAN_A, 0);
    pwm_set_chan_level(slice_num_b_left, PWM_CHAN_B, (uint16_t)-motor_left_duty);
  }
  // Motor Sleep
  if (motor_right_duty || motor_left_duty) {
    motor_driver_sleep = false;
  }

  if (DEBUG_PRINT) {
    printf("Right Duty: %" PRIi16 ", Left Duty: %" PRIi16 "\n", motor_right_duty, motor_left_duty);
  }

  gpio_put(MOTOR_N_SLEEP_GPIO, motor_driver_sleep ? 0 : 1);
}

static void point(struct MotorCommand *mc, double error) {
  // Determine rotation direction
  if (fabs(error) < POINT_DEADBAND_PLUS_MINUS_DEGREES) {
    // No rotation
    mc->motor_left_duty_cycle = 0.0;
    mc->motor_right_duty_cycle = 0.0;
  } else if (error > 0.0) {
    // Clockwise
    mc->motor_left_duty_cycle = MIN_DUTY_CYCLE;
    mc->motor_right_duty_cycle = 0.0;
  } else {
    // Counter Clockwise
    mc->motor_left_duty_cycle = 0.0;
    mc->motor_right_duty_cycle = MIN_DUTY_CYCLE;
  }
}

static void swim(struct MotorCommand *mc, double error) {
  // PD Controls
  double derivative = error - mc->previous_error;
  double adjustment = mc->Kp * error + mc->Kd * derivative;
  mc->previous_error = error;

  if (DEBUG_PRINT) {
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

  if (DEBUG_PRINT) {
    printf("duty left: %f duty right: %f\n", mc->motor_left_duty_cycle, mc->motor_right_duty_cycle);
  }
}

static double get_heading_offset(QueueHandle_t mailbox, double desired_heading) {
  struct MagXYZ mag = {0};
  xQueuePeek(mailbox, &mag, 0);

  apply_calibration_kasa(&mag);

  double current_heading = get_heading(&mag);
  double angle_diff = desired_heading - current_heading;

  // Normalize the angle difference to be between -180 and 180 degrees
  if (angle_diff > 180.0) {
    angle_diff -= 360.0;
  } else if (angle_diff < -180.0) {
    angle_diff += 360.0;
  }

  return angle_diff;
}

static void execute_motor_algorithm(struct MotorCommand *mc, QueueHandle_t mailbox) {
  double heading_offset = get_heading_offset(mailbox, mc->desired_heading);

  // Perform motor algorithm
  if (mc->remaining_time_ms) {
    switch (mc->type) {
      case MOTOR:
        // No manipulation needed
        break;
      case POINT:
        point(mc, heading_offset);
        break;
      case SWIM:
        swim(mc, heading_offset);
        break;
      case FLOAT:
        // No manipulation needed
        break;
      default:
        memset(mc, 0, sizeof(struct MotorCommand));
    }
  }
}

static void check_motor_stop(struct MotorCommand *mc, SemaphoreHandle_t motor_stop) {
  if (uxSemaphoreGetCount(motor_stop)) {
    // Empty cmd queue - Always returns pass
    xQueueReset(motor_stop);
    // Drop Semaphore to 0
    if (xSemaphoreTake(motor_stop, 0) == pdFALSE) {
      printf("Error: Motor Stop Semaphore");
    }
    // Reset motor command
    memset(mc, 0, sizeof(struct MotorCommand));
    printf("Motor Stopped!\n");
  }
}

static void load_motor_command(struct MotorCommand *mc, QueueHandle_t command_queue) {
  if (xQueueReceive(command_queue, mc, 0)) {
    motor_cmd_rx_count++;
    if (DEBUG_PRINT) {
      printf("Motor msg rx: %" PRIu32 "ms\n", mc->remaining_time_ms);
    }
    printf("Motor Command Type: %" PRIu32 "\n", (uint32_t)mc->type);
  } else {
    memset(mc, 0, sizeof(struct MotorCommand));
  }
}

uint32_t get_motor_command_rx_count() { return motor_cmd_rx_count; }

// Motor Notes:
// >70% duty cycle to turn on
// Turns off <65% or so
// So usable range is between 70% and 100% Duty cycle
void vMotorTask(void *pvParameters) {
  init_motor();

  struct MotorTaskParameters *mq = (struct MotorTaskParameters *)pvParameters;
  struct MotorCommand mc = {0};

  vTaskDelay(1000);

  for (;;) {
    // Check semaphore for halt command
    check_motor_stop(&mc, mq->motor_stop);

    // Load motor command if previous mc expired
    if (mc.remaining_time_ms == 0) {
      load_motor_command(&mc, mq->command_queue);
    }

    // Update motor command based on algorithm choice
    execute_motor_algorithm(&mc, mq->mag_queue);

    // Update PWM and Sleep Pin
    set_motor(&mc);

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
