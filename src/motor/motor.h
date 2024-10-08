#ifndef _DD_MOTOR_H
#define _DD_MOTOR_H

#include "queue.h"
#include "semphr.h"

struct MotorTaskParameters {
  QueueHandle_t command_queue;
  QueueHandle_t mag_queue;
  SemaphoreHandle_t motor_stop;
  SemaphoreHandle_t calibrate;
};

uint32_t get_motor_command_rx_count();
uint32_t get_motor_drv_error_count();
void vMotorTask(void *pvParameters);

#endif