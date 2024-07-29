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

static void publish(mqtt_client_t *client, char *topic, char *payload) {
  err_t err =
      mqtt_publish(client, topic, payload, strlen(payload), 1, 0, mqtt_pub_request_cb, NULL);
  if (err != ERR_OK) {
    printf("Publish err: %d\n", err);
  }
}

static void publish_metric_float(mqtt_client_t *client, char *topic, float metric) {
  char metric_payload[64] = {0};
  snprintf(metric_payload, sizeof(metric_payload), "%.4f", metric);
  publish(client, topic, metric_payload);
}

// static void publish_metric_int(PublishTaskHandle *handle, char *topic, int metric) {
//   char metric_payload[64] = {0};
//   snprintf(metric_payload, sizeof(metric_payload), "%d", metric);
//   publish(handle, topic, metric_payload);
// }

static void publish_mag(mqtt_client_t *client, char *topic, struct magXYZ *mag_xyz) {
  char mag_payload[64] = {0};
  snprintf(mag_payload, sizeof(mag_payload), "X: %f, Y: %f, Z: %f\n", mag_xyz->x_uT, mag_xyz->y_uT,
           mag_xyz->z_uT);
  publish(client, topic, mag_payload);
}

extern char global_mac_address[32];
static void publish_mac(mqtt_client_t *client, char *topic) {
  char mac_payload[64] = {0};
  snprintf(mac_payload, sizeof(mac_payload), "MAC: %s\n", global_mac_address);
  publish(client, topic, mac_payload);
}

static void sensor_topic(char *topic_buffer, size_t length, const char *sensor) {
  snprintf(topic_buffer, length, "%s/devices/%d/sensor/%s", DANCING_DUCK_SUBSCRIPTION, DUCK_ID_NUM,
           sensor);
}

/* Task to publish status periodically */
void vPublishTask(void *pvParameters) {
  struct publishTaskParameters *params = (struct publishTaskParameters *)pvParameters;

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

  // Create MAC topic
  char mac_topic[64] = {0};
  snprintf(mac_topic, sizeof(mac_topic), "%s/devices/%d/mac", DANCING_DUCK_SUBSCRIPTION,
           DUCK_ID_NUM);

  unsigned int count = 0;
  char topic_buffer[64] = {0};

  // Todo: remove this, and add semaphore to mqtt_connection_cb
  vTaskDelay(1000);

  for (;;) {
    // 10 Hz - 100ms - Always evaluates to true
    if (count % 1 == 0) {
      // sensor_topic(topic_buffer, sizeof(topic_buffer), "mag");
      // publish_mag(params->client, topic_buffer);
    }
    // 1 Hz - 1000ms
    if (count % 10 == 0) {
      struct magXYZ mag_xyz = {0};
      xQueuePeek(params->mag, &mag_xyz, 0);
      sensor_topic(topic_buffer, sizeof(topic_buffer), "heading");
      publish_metric_float(params->client, topic_buffer, getHeading(&mag_xyz));

      // Used for external calibration
      sensor_topic(topic_buffer, sizeof(topic_buffer), "mag");
      publish_mag(params->client, topic_buffer, &mag_xyz);
    }
    // 0.1 Hz - 10s
    if (count % 100 == 0) {
      publish(params->client, quack_topic, quack_payload);

      sensor_topic(topic_buffer, sizeof(topic_buffer), "temp_rp2040_C");
      publish_metric_float(params->client, topic_buffer, getTemp_C());

      sensor_topic(topic_buffer, sizeof(topic_buffer), "battery_V");
      publish_metric_float(params->client, topic_buffer, getBattery_V());

      publish_mac(params->client, mac_topic);
    }

    count++;
    vTaskDelay(100);
  }
}