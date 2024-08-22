#ifndef _DD_MQTT_H
#define _DD_MQTT_H

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "queue.h"
#include "semphr.h"

#define DANCING_DUCK_SUBSCRIPTION ("dancing_duck")

struct MqttParameters {
  QueueHandle_t motor_queue;
  QueueHandle_t duck_mode_mailbox;
  SemaphoreHandle_t motor_stop;
  SemaphoreHandle_t calibrate;
};

err_t mqtt_connect(mqtt_client_t *client, void *arg);

#endif