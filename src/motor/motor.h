#ifndef _DD_MOTOR_H
#define _DD_MOTOR_H

struct motorQueues {
  QueueHandle_t *command_queue;
  QueueHandle_t *mag_queue;
};

void vMotorTask(void *pvParameters);

#endif