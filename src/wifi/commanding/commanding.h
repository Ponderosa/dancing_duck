#ifndef _DD_COMMANDING_H
#define _DD_COMMANDING_H

#include "FreeRTOS.h"
#include "queue.h"
#include "stdint.h"

typedef struct motorCommand {
  uint16_t version;
  uint16_t motor_1_duty_cycle;
  uint16_t motor_2_duty_cycle;
  uint32_t duration_s;
} motorCommand_t;

void enqueue_motor_command(QueueHandle_t *queue, const uint8_t *data, uint16_t len);

#endif