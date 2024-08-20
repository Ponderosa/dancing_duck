#ifndef _DD_DANCE_GEN_H
#define _DD_DANCE_GEN_H

#include "FreeRTOS.h"

#include "queue.h"
#include "stdint.h"

void init_dance_program();
int get_current_dance();
int get_dance_count();
void dance_generator(QueueHandle_t motor_queue, uint32_t current_second);

#endif