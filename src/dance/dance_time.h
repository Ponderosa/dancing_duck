#ifndef _DD_DANCE_TIME_H
#define _DD_DANCE_TIME_H

#include "stdint.h"

struct DanceTimeParameters {
  QueueHandle_t motor_queue;
  QueueHandle_t duck_mode_mailbox;
};

void reset_dance_time();
void set_dance_server_time_ms(const char *data, uint16_t len);
uint32_t get_dance_server_time_raw_ms();
uint32_t get_dance_server_time_calc_ms();
void vDanceTimeTask(void *pvParameters);

#endif