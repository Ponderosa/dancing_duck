#ifndef _DD_MQTT_H
#define _DD_MQTT_H

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

err_t mqtt_connect(mqtt_client_t *client);
void vMqttPublishStatus(void *pvParameters);

#endif