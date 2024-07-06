#include <string.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "mqtt.h"
#include "publish.h"
#include "task.h"

/* Callback for publish request */
static void mqtt_pub_request_cb(void *arg, err_t result) {
  (void)arg;

  if (result != ERR_OK) {
    printf("Publish result: %d\n", result);
  }
}

/* Task to publish status periodically */
void vPublishTask(void *pvParameters) {
  PublishTaskHandle *handle = (PublishTaskHandle *)pvParameters;
  mqtt_client_t *client = handle->client;

  // Get 64bit Unique ID
  char id[9];  // 8 bytes plus null terminator
  pico_get_unique_board_id_string(id, sizeof(id));
  printf("64-bit NAND FLASH ID: ");
  for (size_t i = 0; i < sizeof(id); i++) {
    printf("%02x ", (unsigned char)id[i]);
  }
  printf("\n");

  // Create Payload
  char pub_payload[64] = {0};
  snprintf(pub_payload, sizeof(pub_payload), "Quack! - %s", id);

  // Create topic
  char pub_topic[64] = {0};
  snprintf(pub_topic, sizeof(pub_topic), "%s/%d/test", KEEP_ALIVE_SUBSCRIPTION, DUCK_ID_NUM);

  for (;;) {
    err_t err = mqtt_publish(client, pub_topic, pub_payload, strlen(pub_payload), 1, 0,
                             mqtt_pub_request_cb, NULL);
    if (err != ERR_OK) {
      printf("Publish err: %d\n", err);
    }
    vTaskDelay(5000);
  }
}