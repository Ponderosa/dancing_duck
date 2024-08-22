#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "cJSON.h"

#include "commanding.h"
#include "config.h"
#include "magnetometer.h"
#include "mqtt.h"
#include "queue.h"
#include "stdint.h"

static uint32_t bad_json_count = 0;
uint32_t motor_queue_error = 0;  // Todo: Figure out how to make this not global

uint32_t get_bad_json_count() { return bad_json_count; }

uint32_t get_motor_queue_error_count() { return motor_queue_error; }

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

static void set_duck_mode(struct MqttParameters *mp, enum DuckMode dm) {
  xQueueOverwrite(mp->duck_mode_mailbox, &dm);
  xQueueReset(mp->motor_queue);
  if (xSemaphoreGive(mp->motor_stop) == pdFALSE) {
    printf("Error: Semaphore give motor stop\n");
  }
}

void enqueue_calibrate_command(struct MqttParameters *mp) {
  set_duck_mode(mp, CALIBRATE);

  struct MotorCommand mc = {0};

  mc.type = MOTOR;
  mc.motor_left_duty_cycle = MID_DUTY_CYCLE;
  mc.remaining_time_ms = KASA_CALIBRATION_TIME_MS;

  if (xQueueSendToBack(mp->motor_queue, &mc, 0) != pdTRUE) {
    motor_queue_error++;
  }

  if (xSemaphoreGive(mp->calibrate) == pdFALSE) {
    printf("Error: Semaphore give calibrate\n");
  }
}

// This function has early exits
void enqueue_launch_command(struct MqttParameters *mp, const char *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

  if (json == NULL) {
    bad_json_count++;
    return;  // Early Exit!
  }

  print_json(data, json);

  double launch_time_s;
  double launch_heading;

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

  set_duck_mode(mp, LAUNCH);

  struct MotorCommand mc = {0};

  if (is_calibrated()) {
    mc.type = SWIM;
    mc.Kp = Kp;
    mc.Kd = Kd;
    mc.desired_heading = launch_heading;
  } else {
    mc.type = MOTOR;
    mc.motor_left_duty_cycle = MID_DUTY_CYCLE;
    mc.motor_right_duty_cycle = MID_DUTY_CYCLE;
  }

  mc.remaining_time_ms = (uint32_t)launch_time_s * 1000;

  if (xQueueSendToBack(mp->motor_queue, &mc, 0) != pdTRUE) {
    motor_queue_error++;
  }
}

void set_dance_mode(struct MqttParameters *mp) { set_duck_mode(mp, DANCE); }

// This function has early exits
void enqueue_motor_command(struct MqttParameters *mp, const char *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

  if (json == NULL) {
    bad_json_count++;
    return;  // Early Exit!
  }

  print_json(data, json);

  int num;
  double num_f;
  struct MotorCommand mc = {0};

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
    default:
      free_bad_json(json);
      return;  // Early Exit!
  }

  if (json_get_int(json, "dur_ms", &num)) {
    printf("Error reading duration in milliseconds\n");
    free_bad_json(json);
    return;  // Early Exit!
  } else {
    mc.remaining_time_ms = num;
  }

  set_duck_mode(mp, OVERRIDE);

  if (xQueueSendToBack(mp->motor_queue, &mc, 0) != pdTRUE) {
    motor_queue_error++;
  }

  cJSON_Delete(json);
}

void set_stop_mode(struct MqttParameters *mp) { set_duck_mode(mp, STOP); }