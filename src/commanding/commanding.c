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

// This function has early exits!
void enqueue_motor_command(QueueHandle_t *queue, const char *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

  if (JSON_DEBUG) {
    char *string = cJSON_Print(json);
    if (string == NULL) {
      printf("Failed to print json.\n");
    } else {
      printf("JSON:\n%s\n", string);
    }
  }

  motorCommand_t *mc = (motorCommand_t *)pvPortMalloc(sizeof(motorCommand_t));
  if (!mc) {
    printf("Error: Malloc failed - Motor Command");
    goto end;
  }

  int num;
  if (json_get_int(json, "motor_1_duty_cycle", &num)) {
    printf("Error reading motor_1_duty_cycle");
    goto end;
  } else {
    mc->motor_1_duty_cycle = num;
  }

  if (json_get_int(json, "motor_2_duty_cycle", &num)) {
    printf("Error reading motor_2_duty_cycle");
    goto end;
  } else {
    mc->motor_2_duty_cycle = num;
  }

  if (json_get_int(json, "duration_s", &num)) {
    printf("Error reading duration_s");
    goto end;
  } else {
    mc->duration_s = num;
  }

  xQueueSendToBack(*queue, mc, 0);

end:
  vPortFree(mc);
  cJSON_Delete(json);
}