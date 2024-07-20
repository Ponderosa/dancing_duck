#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "cJSON.h"

#include "commanding.h"
#include "config.h"
#include "queue.h"
#include "stdint.h"

static bool json_get_int(cJSON *json, const char *name, int *ret_val) {
  cJSON *num = cJSON_GetObjectItemCaseSensitive(json, name);
  if (cJSON_IsNumber(num)) {
    *ret_val = num->valueint;
    return 0;
  }
  return 1;
}

static bool json_get_float(cJSON *json, const char *name, float *ret_val) {
  cJSON *num = cJSON_GetObjectItemCaseSensitive(json, name);
  if (cJSON_IsNumber(num)) {
    *ret_val = (float)(num->valuedouble);
    return 0;
  }
  return 1;
}

// This function has early exits!
void enqueue_motor_command(QueueHandle_t *queue, const char *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

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

  struct motorCommand mc = {0};

  int num;
  float num_f;

  if (json_get_int(json, "type", &num)) {
    printf("Error reading type");
    goto end;
  } else {
    mc.type = (enum motorCommandType)num;
  }

  switch (mc.type) {
    case MOTOR:
      if (json_get_float(json, "duty_right", &num_f)) {
        printf("Error reading right motor duty cycle");
        goto end;
      } else {
        mc.motor_right_duty_cycle = num_f;
      }

      if (json_get_float(json, "duty_left", &num_f)) {
        printf("Error reading left motor duty cycle");
        goto end;
      } else {
        mc.motor_left_duty_cycle = num_f;
      }
      break;
    case SWIM:
      if (json_get_float(json, "Kp", &num_f)) {
        printf("Error reading Kp");
        goto end;
      } else {
        mc.Kp = num_f;
      }

      if (json_get_float(json, "Kd", &num_f)) {
        printf("Error reading Kd");
        goto end;
      } else {
        mc.Kd = num_f;
      }
      // Fall through!
    case POINT:
      if (json_get_float(json, "heading", &num_f)) {
        printf("Error reading heading");
        goto end;
      } else {
        mc.desired_heading = num_f;
      }
      break;
    default:
      goto end;
  }

  if (json_get_int(json, "dur_ms", &num)) {
    printf("Error reading duration in milliseconds");
    goto end;
  } else {
    mc.remaining_time_ms = num;
  }

  xQueueSendToBack(*queue, &mc, 0);

end:
  cJSON_Delete(json);
}