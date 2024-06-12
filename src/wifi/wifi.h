#ifndef _DD_WIFI_H
#define _DD_WIFI_H

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

void vScanWifi();
void vPing();
void example_do_connect(mqtt_client_t*);

#endif