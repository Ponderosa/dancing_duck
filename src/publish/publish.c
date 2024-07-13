#include <string.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "adc.h"
#include "magnetometer.h"
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

static void publish(PublishTaskHandle *handle, char *topic, char *payload) {
  err_t err = mqtt_publish(handle->client, topic, payload, strlen(payload), 1, 0,
                           mqtt_pub_request_cb, NULL);
  if (err != ERR_OK) {
    printf("Publish err: %d\n", err);
  }
}

static void publish_mag(PublishTaskHandle *handle, char *topic) {
  MagXYZ mag_xyz = {0};
  xQueuePeek(handle->mag, &mag_xyz, 0);
  char mag_payload[64] = {0};
  snprintf(mag_payload, sizeof(mag_payload), "X: %f, Y: %f, Z: %f\n", mag_xyz.x_uT, mag_xyz.y_uT,
           mag_xyz.z_uT);
  publish(handle, topic, mag_payload);
}

static void publish_batt(PublishTaskHandle *handle, char *topic) {
  char batt_payload[64] = {0};
  snprintf(batt_payload, sizeof(batt_payload), "Battery: %fV", getBattery_V());
  publish(handle, topic, batt_payload);
}

static void publish_temp(PublishTaskHandle *handle, char *topic) {
  char temp_payload[64] = {0};
  snprintf(temp_payload, sizeof(temp_payload), "Temp: %fC", getTemp_C());
  publish(handle, topic, temp_payload);
}

/* Task to publish status periodically */
void vPublishTask(void *pvParameters) {
  PublishTaskHandle *handle = (PublishTaskHandle *)pvParameters;

  // Get 64bit Unique ID
  char id[9];  // 8 bytes plus null terminator
  pico_get_unique_board_id_string(id, sizeof(id));
  printf("64-bit NAND FLASH ID: ");
  for (size_t i = 0; i < sizeof(id); i++) {
    printf("%02x ", (unsigned char)id[i]);
  }
  printf("\n");

  // Create Quack Payload
  char quack_payload[64] = {0};
  snprintf(quack_payload, sizeof(quack_payload), "Quack! - %s", id);

  // Create Quack topic
  char quack_topic[64] = {0};
  snprintf(quack_topic, sizeof(quack_topic), "%s/devices/%d/quack", DANCING_DUCK_SUBSCRIPTION,
           DUCK_ID_NUM);

  // Create Mag Topic
  char mag_topic[64] = {0};
  snprintf(mag_topic, sizeof(mag_topic), "%s/devices/%d/sensor/mag", DANCING_DUCK_SUBSCRIPTION,
           DUCK_ID_NUM);

  // Create Battery Topic
  char battery_topic[64] = {0};
  snprintf(battery_topic, sizeof(battery_topic), "%s/devices/%d/sensor/battery",
           DANCING_DUCK_SUBSCRIPTION, DUCK_ID_NUM);

  // Create Temp Topic
  char temp_topic[64] = {0};
  snprintf(temp_topic, sizeof(temp_topic), "%s/devices/%d/sensor/temp", DANCING_DUCK_SUBSCRIPTION,
           DUCK_ID_NUM);

  unsigned int count = 0;

  // Todo: remove this, and add semaphore to mqtt_connection_cb
  vTaskDelay(1000);

  for (;;) {
    // 10 Hz - 100ms - Always evaluates to true
    if (count % 1 == 0) {
      // publish_mag(handle, mag_topic);
    }
    // 1 Hz - 1000ms
    if (count % 10 == 0) {
    }
    // 0.1 Hz - 10s
    if (count % 100 == 0) {
      publish(handle, quack_topic, quack_payload);
      publish_mag(handle, mag_topic);
      publish_batt(handle, battery_topic);
      publish_temp(handle, temp_topic);
    }

    count++;
    vTaskDelay(100);
  }
}