#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "cJSON.h"

#include "commanding.h"
#include "config.h"
#include "queue.h"
#include "stdint.h"

static uint32_t bad_json_count = 0;

static bool json_get_int(const cJSON *json, const char *name, int *ret_val) {
  cJSON *num = cJSON_GetObjectItemCaseSensitive(json, name);
  if (cJSON_IsNumber(num)) {
    *ret_val = num->valueint;
    return 0;
  }
  return 1;
}

static bool json_get_float(const cJSON *json, const char *name, float *ret_val) {
  cJSON *num = cJSON_GetObjectItemCaseSensitive(json, name);
  if (cJSON_IsNumber(num)) {
    *ret_val = (float)(num->valuedouble);
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
  float num_f;

  if (json_get_int(json, "type", &num)) {
    printf("Error reading type\n");
    free_bad_json(json);
    return;  // Early Exit!
  } else {
    mc.type = (enum MotorCommandType)num;
  }

  switch (mc.type) {
    case MOTOR:
      if (json_get_float(json, "duty_right", &num_f)) {
        printf("Error reading right motor duty cycle\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.motor_right_duty_cycle = num_f;
      }

      if (json_get_float(json, "duty_left", &num_f)) {
        printf("Error reading left motor duty cycle\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.motor_left_duty_cycle = num_f;
      }
      break;
    case SWIM:
      if (json_get_float(json, "Kp", &num_f)) {
        printf("Error reading Kp\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.Kp = num_f;
      }

      if (json_get_float(json, "Kd", &num_f)) {
        printf("Error reading Kd\n");
        free_bad_json(json);
        return;  // Early Exit!
      } else {
        mc.Kd = num_f;
      }
      // Fall through!
    case POINT:
      if (json_get_float(json, "heading", &num_f)) {
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
      // Todo: Implement return to dock
      free_bad_json(json);
      return;  // Early Exit!
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

  xQueueSendToBack(queue, &mc, 0);

  cJSON_Delete(json);
}