#include <inttypes.h>
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
#include "lis2mdl.h"
#include "magnetometer.h"
#include "motor.h"
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

static void publish(mqtt_client_t *client, const char *topic, const char *payload) {
  char topic_buffer[128];
  snprintf(topic_buffer, sizeof(topic_buffer), "%s/devices/%" PRIu32 "/%s",
           DANCING_DUCK_SUBSCRIPTION, (uint32_t)DUCK_ID_NUM, topic);

  // Acquire locks
  cyw43_arch_lwip_begin();
  err_t err =
      mqtt_publish(client, topic_buffer, payload, strlen(payload), 1, 0, mqtt_pub_request_cb, NULL);
  cyw43_arch_lwip_end();

  if (err != ERR_OK) {
    printf("Publish err: %d\n", err);
    publish_error_count++;
    continuous_publish_error_count++;
  } else {
    continuous_publish_error_count = 0;
  }
}

static void publish_float(mqtt_client_t *client, const char *topic, double val) {
  char payload[64] = {0};
  snprintf(payload, sizeof(payload), "%.4f", val);
  publish(client, topic, payload);
}

static void publish_int(mqtt_client_t *client, const char *topic, int32_t val) {
  char payload[64] = {0};
  snprintf(payload, sizeof(payload), "%" PRIi32 "", val);
  publish(client, topic, payload);
}

extern char global_mac_address[32];
// Todo: change to log
static void publish_mac(mqtt_client_t *client) {
  // Create MAC topic
  char mac_topic[64] = {0};
  snprintf(mac_topic, sizeof(mac_topic), "%s/devices/%" PRIu32 "/mac", DANCING_DUCK_SUBSCRIPTION,
           (uint32_t)DUCK_ID_NUM);
  char mac_payload[64] = {0};
  snprintf(mac_payload, sizeof(mac_payload), "MAC: %s\n", global_mac_address);
  publish(client, mac_topic, mac_payload);
}

static void publish_rssi(mqtt_client_t *client) {
  int32_t rssi;
  if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == PICO_OK) {
    publish_int(client, "metric/rssi", rssi);
  }
}

static void publish_magnetometer_metrics(struct PublishTaskParameters *params) {
  struct MagXYZ mag_xyz = {0};
  xQueuePeek(params->mag, &mag_xyz, 0);

  publish_float(params->client, "sensor/mag_x_uT", mag_xyz.x_uT);
  publish_float(params->client, "sensor/mag_y_uT", mag_xyz.y_uT);
  publish_float(params->client, "sensor/mag_z_uT", mag_xyz.z_uT);

  apply_calibration_kasa(&mag_xyz);

  publish_float(params->client, "sensor/mag_calibrated_x_uT", mag_xyz.x_uT);
  publish_float(params->client, "sensor/mag_calibrated_y_uT", mag_xyz.y_uT);
  publish_float(params->client, "sensor/heading", get_heading(&mag_xyz));

  struct CircleCenter cr;
  get_kasa_raw(&cr);
  publish_float(params->client, "metric/kasa_rmse", cr.rmse);
}

/* Task to publish status periodically */
void vPublishTask(void *pvParameters) {
  struct PublishTaskParameters *params = (struct PublishTaskParameters *)pvParameters;

  // Get 64bit Unique ID
  // Todo: Make this a boot log
  char id[9];  // 8 bytes plus null terminator
  pico_get_unique_board_id_string(id, sizeof(id));
  printf("64-bit NAND FLASH ID: ");
  for (size_t i = 0; i < sizeof(id); i++) {
    printf("%02x ", (unsigned char)id[i]);
  }
  printf("\n");

  // Todo: remove this, and add semaphore to mqtt_connection_cb
  vTaskDelay(1000);

  // Publish boot metrics
  publish_int(params->client, "metric/boot_count", bootCount());
  publish_int(params->client, "metric/soft_reboot_reason", rebootReasonSoft());
  publish_int(params->client, "metric/hard_reboot_reason", rebootReasonHard());
  publish_mac(params->client);

  vTaskDelay(1000);

  unsigned int count = 0;

  for (;;) {
    check_continuous_error_count();

    // 10 Hz - 100ms - Always evaluates to true
    if (count % 1 == 0) {
      // publish_magnetometer_metrics(params);

      // Performance testing
      // for (int i = 0; i < 10; i++) {
      //   publish_mac(params->client, mac_topic);

      //   metric_topic(topic_buffer, sizeof(topic_buffer), "mqtt_pub_err_cnt");
      //   publish_int(params->client, topic_buffer, publish_error_count);

      //   metric_topic(topic_buffer, sizeof(topic_buffer), "mqtt_pub_cb_err_cnt");
      //   publish_int(params->client, topic_buffer, callback_error_count);
      // }
    }
    // 1 Hz - 1000ms
    if (count % 10 == 0) {
      publish_magnetometer_metrics(params);
      publish_rssi(params->client);
      publish_int(params->client, "metric/mqtt_pub_err_cnt", publish_error_count);
    }
    // 0.1 Hz - 10s
    if (count % 100 == 0) {
      publish_float(params->client, "sensor/temp_rp2040_C", get_temp_C());
      publish_float(params->client, "sensor/battery_V", get_battery_V());
      publish_int(params->client, "metric/mqtt_pub_cb_err_cnt", callback_error_count);
      publish_int(params->client, "metric/bad_json_count", get_bad_json_count());
      publish_int(params->client, "metric/motor_cmd_rx_cnt", get_motor_command_rx_count());
      publish_int(params->client, "metric/motor_queue_error_cnt", get_motor_queue_error_count());
      publish_int(params->client, "metric/set_mag_mb_err_cnt", get_mag_mailbox_set_error_count());
      publish_int(params->client, "metric/mag_cfg_err_cnt", get_config_fail_count());
    }

    count++;
    vTaskDelay(100);
  }
}