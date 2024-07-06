#ifndef _DD_PUBLISH_H
#define _DD_PUBLISH_H

#include "FreeRTOS.h"

#include "queue.h"

typedef struct publish_task_handle {
  mqtt_client_t *client;
  QueueHandle_t *mag;
} PublishTaskHandle;

void vPublishTask(void *pvParameters);

#endif