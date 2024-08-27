#include <string.h>

#include "FreeRTOS.h"

#include "pico/printf.h"

#include "commanding.h"
#include "config.h"
#include "dance_time.h"
#include "queue.h"
#include "stdint.h"

static const bool DEBUG_PRINT = true;
static const uint32_t DANCE_TRIGGER_INTERVAL_S = 120;
static const size_t NUM_DANCES = 7;

struct DanceRoutine {
  struct MotorCommand *mc_array;
  size_t size;
};

static struct DanceRoutine *dance_program;
static int current_dance = 0;
static int dance_count = 0;
static uint32_t wind_correction_counter = 0;

int get_current_dance() { return current_dance; }

int get_dance_count() { return dance_count; }

static void create_empty_dance_routine(struct DanceRoutine *dance, size_t size) {
  dance->mc_array = (struct MotorCommand *)pvPortMalloc(sizeof(struct MotorCommand) * size);
  memset(dance->mc_array, 0, sizeof(struct MotorCommand) * size);
  dance->size = size;
}

static void create_motor_movement(struct MotorCommand *mc, double right_duty, double left_duty,
                                  uint32_t duration_ms) {
  mc->type = MOTOR;
  mc->motor_right_duty_cycle = right_duty;
  mc->motor_left_duty_cycle = left_duty;
  mc->remaining_time_ms = duration_ms;
}

static void create_point_movement(struct MotorCommand *mc, double heading, uint32_t duration_ms) {
  mc->type = POINT;
  mc->desired_heading = heading;
  mc->remaining_time_ms = duration_ms;
}

static void create_swim_movement(struct MotorCommand *mc, double heading, uint32_t duration_ms) {
  mc->type = SWIM;
  mc->desired_heading = heading;
  mc->Kp = Kp;
  mc->Kd = Kd;
  mc->remaining_time_ms = duration_ms;
}

static void create_float_movement(struct MotorCommand *mc, uint32_t duration_ms) {
  mc->type = FLOAT;
  mc->remaining_time_ms = duration_ms;
}

static void create_box_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 8);
  create_swim_movement(&dance->mc_array[0], 360.0, 20000);
  create_float_movement(&dance->mc_array[1], 10000);
  create_swim_movement(&dance->mc_array[2], 270.0, 20000);
  create_float_movement(&dance->mc_array[3], 10000);
  create_swim_movement(&dance->mc_array[4], 180.0, 20000);
  create_float_movement(&dance->mc_array[5], 10000);
  create_swim_movement(&dance->mc_array[6], 90.0, 20000);
  create_float_movement(&dance->mc_array[7], 10000);
}

static void create_back_and_forth_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 3);
  create_motor_movement(&dance->mc_array[0], MID_DUTY_CYCLE, 0.0, 20000);
  create_float_movement(&dance->mc_array[1], 15000);
  create_motor_movement(&dance->mc_array[2], 0.0, MID_DUTY_CYCLE, 20000);
}

static void create_spin_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 3);
  create_motor_movement(&dance->mc_array[0], MID_DUTY_CYCLE, 0.0, 20000);
  create_float_movement(&dance->mc_array[1], 5000);
  create_motor_movement(&dance->mc_array[2], MID_DUTY_CYCLE, 0.0, 20000);
}

static void create_point_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 8);
  create_point_movement(&dance->mc_array[0], 45.0, 25000);
  create_float_movement(&dance->mc_array[1], 10000);
  create_point_movement(&dance->mc_array[2], 135.0, 25000);
  create_float_movement(&dance->mc_array[3], 10000);
  create_point_movement(&dance->mc_array[4], 225.0, 25000);
  create_float_movement(&dance->mc_array[5], 10000);
  create_point_movement(&dance->mc_array[6], 315.0, 25000);
  create_float_movement(&dance->mc_array[7], 10000);
}

static void create_figure_eight_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 8);
  create_motor_movement(&dance->mc_array[0], MID_DUTY_CYCLE, 0.0, 10000);
  create_motor_movement(&dance->mc_array[1], 0.0, MID_DUTY_CYCLE, 10000);
  create_motor_movement(&dance->mc_array[2], MID_DUTY_CYCLE, 0.0, 10000);
  create_motor_movement(&dance->mc_array[3], 0.0, MID_DUTY_CYCLE, 10000);
  create_motor_movement(&dance->mc_array[4], MID_DUTY_CYCLE, 0.0, 10000);
  create_motor_movement(&dance->mc_array[5], 0.0, MID_DUTY_CYCLE, 10000);
  create_motor_movement(&dance->mc_array[6], MID_DUTY_CYCLE, 0.0, 10000);
  create_motor_movement(&dance->mc_array[7], 0.0, MID_DUTY_CYCLE, 10000);
}

static void create_up_and_down_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 3);
  create_swim_movement(&dance->mc_array[0], 360.0, 20000);
  create_float_movement(&dance->mc_array[1], 2500);
  create_swim_movement(&dance->mc_array[2], 180.0, 20000);
}

static void create_side_to_side_dance(struct DanceRoutine *dance) {
  create_empty_dance_routine(dance, 3);
  create_swim_movement(&dance->mc_array[0], 90.0, 20000);
  create_float_movement(&dance->mc_array[1], 2500);
  create_swim_movement(&dance->mc_array[2], 270.0, 20000);
}

void init_dance_program() {
  dance_program = (struct DanceRoutine *)pvPortMalloc(sizeof(struct DanceRoutine) * NUM_DANCES);
  create_box_dance(&dance_program[0]);
  create_back_and_forth_dance(&dance_program[1]);
  create_point_dance(&dance_program[2]);
  create_spin_dance(&dance_program[3]);
  create_up_and_down_dance(&dance_program[4]);
  create_figure_eight_dance(&dance_program[5]);
  create_side_to_side_dance(&dance_program[6]);
}

static uint32_t time_based_prng(uint32_t time_seconds) {
  // Constants for the linear congruential generator - Numerical Recipes in C
  const uint32_t a = 1664525;
  const uint32_t c = 1013904223;

  // Seed the generator with the time
  uint32_t state = time_seconds;

  // Perform a single iteration of the linear congruential generator
  state = a * state + c;

  // Return the generated pseudo-random number
  return state;
}

extern uint32_t motor_queue_error;

uint32_t get_wind_correction_counter() { return wind_correction_counter; }

void wind_correction_generator(struct WindCorrection *wc, QueueHandle_t motor_queue,
                               uint32_t current_second) {
  if (wc->enabled && (current_second % wc->correction_interval_s == 0)) {
    struct MotorCommand mc = {0};
    create_swim_movement(&mc, wc->windward_direction, wc->correction_duration_s * 1000);
    if (xQueueSendToBack(motor_queue, &mc, 0) != pdTRUE) {
      motor_queue_error++;
    } else {
      wind_correction_counter++;
    }
    if (DEBUG_PRINT) {
      printf("Wind correction of %f degrees, for %" PRIu32 " seconds\n", wc->windward_direction,
             wc->correction_duration_s);
    }
  }
}

void dance_generator(QueueHandle_t motor_queue, uint32_t current_second) {
  // Send periodically
  if (current_second % DANCE_TRIGGER_INTERVAL_S == 0) {
    int dance_index;
    if (current_second % (DANCE_TRIGGER_INTERVAL_S * 2) == 0) {
      // Synchronized Dance
      dance_index = (current_second / (DANCE_TRIGGER_INTERVAL_S * 2)) % NUM_DANCES;
      if (DEBUG_PRINT) {
        printf("Synchronized Dance Sent\n");
      }
    } else {
      // Free Style Dance
      dance_index = time_based_prng(xTaskGetTickCount()) % NUM_DANCES;
      if (DEBUG_PRINT) {
        printf("Free Style Dance Sent\n");
      }
    }

    struct DanceRoutine *dance = &dance_program[dance_index];

    for (size_t i = 0; i < dance->size; i++) {
      if (xQueueSendToBack(motor_queue, &dance->mc_array[i], 0) != pdTRUE) {
        motor_queue_error++;
      }
    }

    current_dance = dance_index;
    dance_count++;
  }
}