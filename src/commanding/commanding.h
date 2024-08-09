#ifndef _DD_COMMANDING_H
#define _DD_COMMANDING_H

#include "FreeRTOS.h"

#include "queue.h"
#include "stdint.h"

enum MotorCommandType {
  MOTOR = 0,
  POINT = 1,
  SWIM = 2,
  FLOAT = 3,
  RETURN_TO_DOCK = 4,
  CALIBRATE = 5,
};

struct MotorCommand {
  uint16_t version;
  enum MotorCommandType type;
  double motor_right_duty_cycle;
  double motor_left_duty_cycle;
  double desired_heading;
  double Kp;
  double Kd;
  double previous_error;
  uint32_t remaining_time_ms;
};

void enqueue_motor_command(QueueHandle_t queue, const char *data, uint16_t len);
void enqueue_launch_command(QueueHandle_t queue, const char *data, uint16_t len);
double get_launch_heading();
uint32_t get_bad_json_count();
uint32_t get_motor_queue_error_count();

#endif