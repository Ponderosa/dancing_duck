#ifndef _DD_PUBLISH_H
#define _DD_PUBLISH_H

#include "FreeRTOS.h"

#include "lwip/apps/mqtt.h"

#include "queue.h"

struct PublishTaskParameters {
  mqtt_client_t *client;
  QueueHandle_t mag;
  QueueHandle_t duck_mode_mailbox;
};

void vPublishTask(void *pvParameters);

#endif