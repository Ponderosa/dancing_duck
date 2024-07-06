#ifndef _DD_MQTT_H
#define _DD_MQTT_H

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

/* Messages from duck to commander */
#define KEEP_ALIVE_SUBSCRIPTION    ("KeepAlive")
#define STANDARD_OUT_SUBSCRIPTION  ("StandardOut")
#define ERROR_SUBSCRIPTION         ("ErrorOut")

/* Messaages from commander to duck */
#define DUCK_COMMAND_SUBSCRIPTION  ("DuckCommand")

/* Used for MQTT to UART testing */
#define PRINT_PAYLOAD_SUBSCRIPTION ("PrintPayload")

err_t mqtt_connect(mqtt_client_t *client, void *arg);

#endif