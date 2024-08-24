#ifndef _DD_COMMANDING_H
#define _DD_COMMANDING_H

#include "FreeRTOS.h"

#include "mqtt.h"
#include "queue.h"
#include "stdint.h"

enum MotorCommandType {
  MOTOR = 0,
  POINT = 1,
  SWIM = 2,
  FLOAT = 3,
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

uint32_t get_bad_json_count();
uint32_t get_motor_queue_error_count();

void enqueue_calibrate_command(struct MqttParameters *mp);
void enqueue_launch_command(struct MqttParameters *mp, const char *data, uint16_t len);
void set_dance_mode(struct MqttParameters *mp);
void enqueue_motor_command(struct MqttParameters *mp, const char *data, uint16_t len);
void set_stop_mode(struct MqttParameters *mp);
void set_wind_config(struct MqttParameters *mp, const char *data, uint16_t len);

#endif