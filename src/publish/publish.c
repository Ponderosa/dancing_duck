#include <string.h>

#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/printf.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "adc.h"
#include "commanding.h"
#include "magnetometer.h"
#include "mqtt.h"
#include "publish.h"
#include "reboot.h"
#include "task.h"

static const uint32_t CONTINUOUS_PUBLISH_ERROR_RESET_COUNT = 100;
static const uint32_t CONTINUOUS_CALLBACK_ERROR_RESET_COUNT = 100;

static uint32_t callback_error_count = 0;
static uint32_t continuous_callback_error_count = 0;
static uint32_t publish_error_count = 0;
static uint32_t continuous_publish_error_count = 0;

static void check_continuous_error_count() {
  if ((continuous_publish_error_count > CONTINUOUS_PUBLISH_ERROR_RESET_COUNT) ||
      (continuous_callback_error_count > CONTINUOUS_CALLBACK_ERROR_RESET_COUNT)) {
    reboot(MQTT_PUBLISH_TIMEOUT_REASON);
  }
}

/* Callback for publish request */
static void mqtt_pub_request_cb(void *arg, err_t result) {
  (void)arg;

  if (result != ERR_OK) {
    printf("Publish result: %d\n", result);
    callback_error_count++;
    continuous_callback_error_count++;
  } else {
    continuous_callback_error_count = 0;
  }
}

static void publish(mqtt_client_t *client, char *topic, char *payload) {
  cyw43_arch_lwip_begin();

  err_t err =
      mqtt_publish(client, topic, payload, strlen(payload), 1, 0, mqtt_pub_request_cb, NULL);

  cyw43_arch_lwip_end();

  if (err != ERR_OK) {
    printf("Publish err: %d\n", err);
    publish_error_count++;
    continuous_publish_error_count++;
  } else {
    continuous_publish_error_count = 0;
  }
}

static void publish_metric_float(mqtt_client_t *client, char *topic, double metric) {
  char metric_payload[64] = {0};
  snprintf(metric_payload, sizeof(metric_payload), "%.4f", metric);
  publish(client, topic, metric_payload);
}

static void publish_metric_int(mqtt_client_t *client, char *topic, int32_t metric) {
  char metric_payload[64] = {0};
  snprintf(metric_payload, sizeof(metric_payload), "%ld", metric);
  publish(client, topic, metric_payload);
}

static void publish_mag(mqtt_client_t *client, char *topic, struct MagXYZ *mag_xyz) {
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

static void metric_topic(char *topic_buffer, size_t length, const char *sensor) {
  snprintf(topic_buffer, length, "%s/devices/%d/metric/%s", DANCING_DUCK_SUBSCRIPTION, DUCK_ID_NUM,
           sensor);
}

/* Task to publish status periodically */
void vPublishTask(void *pvParameters) {
  struct PublishTaskParameters *params = (struct PublishTaskParameters *)pvParameters;

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

  // Publish boot metrics
  metric_topic(topic_buffer, sizeof(topic_buffer), "boot_count");
  publish_metric_int(params->client, topic_buffer, bootCount());

  metric_topic(topic_buffer, sizeof(topic_buffer), "soft_reboot_reason");
  publish_metric_int(params->client, topic_buffer, rebootReasonSoft());

  metric_topic(topic_buffer, sizeof(topic_buffer), "hard_reboot_reason");
  publish_metric_int(params->client, topic_buffer, rebootReasonHard());

  vTaskDelay(1000);

  for (;;) {
    check_continuous_error_count();

    // 10 Hz - 100ms - Always evaluates to true
    if (count % 1 == 0) {
      // struct MagXYZ mag_xyz = {0};
      // xQueuePeek(params->mag, &mag_xyz, 0);
      // sensor_topic(topic_buffer, sizeof(topic_buffer), "heading");
      // publish_metric_float(params->client, topic_buffer, get_heading(&mag_xyz));

      // // Used for external calibration
      // sensor_topic(topic_buffer, sizeof(topic_buffer), "mag");
      // publish_mag(params->client, topic_buffer, &mag_xyz);

      // Performance testing
      // for (int i = 0; i < 10; i++) {
      //   publish_mac(params->client, mac_topic);

      //   metric_topic(topic_buffer, sizeof(topic_buffer), "mqtt_pub_err_cnt");
      //   publish_metric_int(params->client, topic_buffer, publish_error_count);

      //   metric_topic(topic_buffer, sizeof(topic_buffer), "mqtt_pub_cb_err_cnt");
      //   publish_metric_int(params->client, topic_buffer, callback_error_count);
      // }
    }
    // 1 Hz - 1000ms
    if (count % 10 == 0) {
      struct MagXYZ mag_xyz = {0};
      xQueuePeek(params->mag, &mag_xyz, 0);
      sensor_topic(topic_buffer, sizeof(topic_buffer), "heading");
      publish_metric_float(params->client, topic_buffer, get_heading(&mag_xyz));

      // Used for external calibration
      sensor_topic(topic_buffer, sizeof(topic_buffer), "mag");
      publish_mag(params->client, topic_buffer, &mag_xyz);

      int32_t rssi;
      if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == PICO_OK) {
        metric_topic(topic_buffer, sizeof(topic_buffer), "rssi");
        publish_metric_int(params->client, topic_buffer, rssi);
      }

      metric_topic(topic_buffer, sizeof(topic_buffer), "mqtt_pub_err_cnt");
      publish_metric_int(params->client, topic_buffer, publish_error_count);
    }
    // 0.1 Hz - 10s
    if (count % 100 == 0) {
      publish(params->client, quack_topic, quack_payload);

      sensor_topic(topic_buffer, sizeof(topic_buffer), "temp_rp2040_C");
      publish_metric_float(params->client, topic_buffer, get_temp_C());

      sensor_topic(topic_buffer, sizeof(topic_buffer), "battery_V");
      publish_metric_float(params->client, topic_buffer, get_battery_V());

      metric_topic(topic_buffer, sizeof(topic_buffer), "mqtt_pub_cb_err_cnt");
      publish_metric_int(params->client, topic_buffer, callback_error_count);

      metric_topic(topic_buffer, sizeof(topic_buffer), "bad_json_count");
      publish_metric_int(params->client, topic_buffer, get_bad_json_count());

      publish_mac(params->client, mac_topic);
    }

    count++;
    vTaskDelay(100);
  }
}