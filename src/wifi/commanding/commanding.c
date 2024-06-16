#include "commanding.h"

#include "FreeRTOS.h"
#include "cJSON.h"
#include "pico/stdlib.h"
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
void enqueue_motor_command(QueueHandle_t *queue, const uint8_t *data, uint16_t len) {
  cJSON *json = cJSON_ParseWithLength(data, len);

  char *string = cJSON_Print(json);
  if (string == NULL) {
    printf("Failed to print json.\n");
  } else {
    printf("JSON:\n%s\n", string);
  }

  motorCommand_t *mc = (motorCommand_t *)malloc(sizeof(motorCommand_t));

  int num;
  if (json_get_int(json, "motor_1_duty_cycle", &num)) {
    printf("Error reading motor_1_duty_cycle");
    free(mc);
    cJSON_Delete(json);
    return;
  } else {
    mc->motor_1_duty_cycle = num;
  }

  if (json_get_int(json, "motor_2_duty_cycle", &num)) {
    printf("Error reading motor_2_duty_cycle");
    free(mc);
    cJSON_Delete(json);
    return;
  } else {
    mc->motor_2_duty_cycle = num;
  }

  if (json_get_int(json, "duration_s", &num)) {
    printf("Error reading duration_s");
    free(mc);
    cJSON_Delete(json);
    return;
  } else {
    mc->duration_s = num;
  }

  printf("motor_1_duty_cycle: %u\n", mc->motor_1_duty_cycle);
  printf("motor_2_duty_cycle: %u\n", mc->motor_2_duty_cycle);
  printf("duration_s: %lu\n", mc->duration_s);

  free(mc);
  cJSON_Delete(json);
}