#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "cJSON.h"

#include "commanding.h"
#include "config.h"
#include "queue.h"
#include "stdint.h"

static uint32_t bad_json_count = 0;
static double launch_heading = 0.0;
uint32_t motor_queue_error = 0;

static bool json_get_int(const cJSON *json, const char *name, int *ret_val) {
  cJSON *num = cJSON_GetObjectItemCaseSensitive(json, name);
  if (cJSON_IsNumber(num)) {
    *ret_val = num->valueint;
    return 0;
  }
  return 1;
}

static bool json_get_double(const cJSON *json, const char *name, double *ret_val) {
  cJSON *num = cJSON_GetObjectItemCaseSensitive(json, name);
  if (cJSON_IsNumber(num)) {
    *ret_val = num->valuedouble;
    return 0;
  }
  return 1;
}

static void print_json(const char *data, const cJSON *json) {
  if (JSON_DEBUG) {
    // Print raw message
    printf("Raw Message: %s.\n", data);

    // Print JSON
    char *string = cJSON_Print(json);
    if (string == NULL) {
      printf("Failed to print json.\n");
    } else {
      printf("JSON:\n%s\n", string);
    }
  }
}

static void free_bad_json(cJSON *json) {
  bad_json_count++;
  cJSON_Delete(json);
}

uint32_t get_bad_json_count() { return bad_json_count; }

uint32_t get_motor_queue_error_count() { return motor_queue_error; }

double get_launch_heading() { return launch_heading; }

// This function has early exits
void enqueue_launch_command(QueueHandle_t queue, const char *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

  if (json == NULL) {
    bad_json_count++;
    return;  // Early Exit!
  }

  print_json(data, json);

  struct MotorCommand mc = {0};

  double launch_time_s;

  if (json_get_double(json, "launch_time", &launch_time_s)) {
    printf("Error reading launch time\n");
    free_bad_json(json);
    return;  // Early Exit!
  }

  if (json_get_double(json, "heading", &launch_heading)) {
    printf("Error reading launch heading\n");
    free_bad_json(json);
    return;  // Early Exit!
  }

  cJSON_Delete(json);

  mc.type = MOTOR;
  mc.motor_left_duty_cycle = MID_DUTY_CYCLE;
  mc.motor_right_duty_cycle = MID_DUTY_CYCLE;
  mc.remaining_time_ms = (uint32_t)launch_time_s * 1000;

  if (xQueueSendToBack(queue, &mc, 0) != pdTRUE) {
    motor_queue_error++;
  }

  memset(&mc, 0, sizeof(struct MotorCommand));

  mc.type = CALIBRATE;
  mc.remaining_time_ms = KASA_CALIBRATION_TIME_MS;

  if (xQueueSendToBack(queue, &mc, 0) != pdTRUE) {
    motor_queue_error++;
  }
}

// This function has early exits
void enqueue_motor_command(QueueHandle_t queue, const char *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

  if (json == NULL) {
    bad_json_count++;
    return;  // Early Exit!
  }

  print_json(data, json);

  struct MotorCommand mc = {0};

  int num;
  double num_f;

  if (json_get_int(json, "type", &num)) {
    printf("Error reading type\n");
    free_bad_json(json);
    return;  // Early Exit!
  } else {
    mc.type = (enum MotorCommandType)num;
  }

  switch (mc.type) {
    case MOTOR:
      if (json_get_double(json, "duty_right", &num_f)) {
        printf("Error reading right motor duty cycle\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.motor_right_duty_cycle = num_f;
      }

      if (json_get_double(json, "duty_left", &num_f)) {
        printf("Error reading left motor duty cycle\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.motor_left_duty_cycle = num_f;
      }
      break;
    case SWIM:
      if (json_get_double(json, "Kp", &num_f)) {
        printf("Error reading Kp\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.Kp = num_f;
      }

      if (json_get_double(json, "Kd", &num_f)) {
        printf("Error reading Kd\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.Kd = num_f;
      }
      // Fall through!
    case POINT:
      if (json_get_double(json, "heading", &num_f)) {
        printf("Error reading heading\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.desired_heading = num_f;
      }
      break;
    case FLOAT:
      // All zeroes
      break;
    case RETURN_TO_DOCK:
      // Todo: handle in Python controller by generating
      // SWIM command to configured heading for configured time
      // It it makes sense, delete this state in the embedded code
      free_bad_json(json);
      return;  // Early Exit!
      break;
    case CALIBRATE:
      // All we need is type and time
      mc.remaining_time_ms = KASA_CALIBRATION_TIME_MS;
      break;
    default:
      free_bad_json(json);
      return;  // Early Exit!
  }

  if (mc.type != CALIBRATE) {
    if (json_get_int(json, "dur_ms", &num)) {
      printf("Error reading duration in milliseconds\n");
      free_bad_json(json);
      return;  // Early Exit!
    } else {
      mc.remaining_time_ms = num;
    }
  }

  if (xQueueSendToBack(queue, &mc, 0) != pdTRUE) {
    motor_queue_error++;
  }

  cJSON_Delete(json);
}