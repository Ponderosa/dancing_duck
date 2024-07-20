#ifndef _DD_COMMANDING_H
#define _DD_COMMANDING_H

#include "FreeRTOS.h"

#include "queue.h"
#include "stdint.h"

enum motorCommandType {
  MOTOR,
  POINT,
  SWIM,
};

struct motorCommand {
  uint16_t version;
  enum motorCommandType type;
  float motor_right_duty_cycle;
  float motor_left_duty_cycle;
  float desired_heading;
  float Kp;
  float Kd;
  float previous_error;
  uint32_t remaining_time_ms;
};

void enqueue_motor_command(QueueHandle_t *queue, const char *data, uint16_t len);

#endif