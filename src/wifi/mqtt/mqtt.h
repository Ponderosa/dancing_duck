#ifndef _DD_MQTT_H
#define _DD_MQTT_H

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#define DANCING_DUCK_SUBSCRIPTION ("dancing_duck")

err_t mqtt_connect(mqtt_client_t *client, void *arg);

#endif