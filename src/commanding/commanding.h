#ifndef _DD_COMMANDING_H
#define _DD_COMMANDING_H

#include "FreeRTOS.h"

#include "queue.h"
#include "stdint.h"

typedef struct motorCommand {
  uint16_t version;
  float motor_right_duty_cycle;
  float motor_left_duty_cycle;
  uint32_t duration_ms;
} motorCommand_t;

void enqueue_motor_command(QueueHandle_t *queue, const char *data, uint16_t len);

#endif