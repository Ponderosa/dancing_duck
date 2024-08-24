#ifndef _DD_DANCE_GEN_H
#define _DD_DANCE_GEN_H

#include "FreeRTOS.h"

#include "queue.h"
#include "stdint.h"

struct WindCorrection {
  double windward_direction;
  uint32_t correction_duration_s;
  uint32_t correction_interval_s;
  bool enabled;
};

void init_dance_program();
int get_current_dance();
int get_dance_count();
uint32_t get_wind_correction_counter();
void dance_generator(QueueHandle_t motor_queue, uint32_t current_second);
void wind_correction_generator(struct WindCorrection *wc, QueueHandle_t motor_queue,
                               uint32_t current_second);

#endif