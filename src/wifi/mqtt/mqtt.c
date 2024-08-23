#include <string.h>

#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/printf.h"
#include "pico/stdlib.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "commanding.h"
#include "dance_time.h"
#include "mqtt.h"
#include "picowota/reboot.h"
#include "reboot.h"
#include "task.h"

#define IP_ADDR0     (MQTT_BROKER_IP_A)
#define IP_ADDR1     (MQTT_BROKER_IP_B)
#define IP_ADDR2     (MQTT_BROKER_IP_C)
#define IP_ADDR3     (MQTT_BROKER_IP_D)

#define IP_ADDR0_ALT (MQTT_BROKER_IP_A_ALT)
#define IP_ADDR1_ALT (MQTT_BROKER_IP_B_ALT)
#define IP_ADDR2_ALT (MQTT_BROKER_IP_C_ALT)
#define IP_ADDR3_ALT (MQTT_BROKER_IP_D_ALT)

/**** Incoming Messages ****/
#define BUFFER_SIZE  128

static const bool DEBUG_PRINT = false;
static uint32_t mqtt_rx_count = 0;

uint32_t get_mqtt_rx_count() { return mqtt_rx_count; }

static int strcmp_formatted(const char *topic, const char *format, ...) {
  char formatted[BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(formatted, BUFFER_SIZE, format, args);
  va_end(args);
  return strcmp(topic, formatted);
}

/* File scoped variable to store the incoming publish ID */
static int inpub_id;

/* Callback for incoming publish */
static void mqtt_incoming_publish_cb(void *params, const char *topic, u32_t tot_len) {
  (void)params;

  if (DEBUG_PRINT) {
    printf("Incoming publish at topic %s with total length %u\n", topic, (unsigned int)tot_len);
  }

  mqtt_rx_count++;

  if (strcmp_formatted(topic, "%s/all_devices/command/uart_tx", DANCING_DUCK_SUBSCRIPTION) == 0) {
    inpub_id = 0;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/calibrate", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 1;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/launch", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 2;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/dance", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 3;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/motor", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 4;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/stop_all", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 5;
  } else if (strcmp_formatted(topic, "%s/all_devices/command/set_time",
                              DANCING_DUCK_SUBSCRIPTION) == 0) {
    inpub_id = 6;
  } else if (strcmp_formatted(topic, "%s/all_devices/command/set_wind",
                              DANCING_DUCK_SUBSCRIPTION) == 0) {
    inpub_id = 7;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/reset", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 9;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/bootloader", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 9;
  } else {
    inpub_id = 10;
  }
}

/* Callback for incoming data */
static void mqtt_incoming_data_cb(void *params, const u8_t *data, u16_t len, u8_t flags) {
  if (DEBUG_PRINT) {
    printf("Incoming publish payload with length %d, flags %u\n", len, (unsigned int)flags);
  }

  struct MqttParameters *mqtt_params = (struct MqttParameters *)params;

  if (flags & MQTT_DATA_FLAG_LAST) {
    if (inpub_id == 0) {
      if (data[len - 1] == 0) {
        printf("UART Test: %s\n", (const char *)data);
      } else {
        printf("Termination check failed \n");
      }
    } else if (inpub_id == 1) {
      printf("Calibrate Command Received\n");
      enqueue_calibrate_command(mqtt_params);
    } else if (inpub_id == 2) {
      printf("Launch Command Received\n");
      enqueue_launch_command(mqtt_params, (char *)data, len);
    } else if (inpub_id == 3) {
      printf("Dance Command Received\n");
      set_dance_mode(mqtt_params);
    } else if (inpub_id == 4) {
      printf("Motor Command Received\n");
      enqueue_motor_command(mqtt_params, (char *)data, len);
    } else if (inpub_id == 5) {
      printf("Stop All Command Received\n");
      set_stop_mode(mqtt_params);
    } else if (inpub_id == 6) {
      printf("Time Update Received\n");
      set_dance_server_time_ms((char *)data, len);
    } else if (inpub_id == 7) {
      printf("Wind Config Received\n");
      // set_wind_config((char *) data, len);
    } else if (inpub_id == 8) {
      printf("Reboot Command received\n");
      reboot(MQTT_COMMANDED_REASON);
    } else if (inpub_id == 9) {
      printf("Bootloader Command Received\n");
      printf("Data: %u", *data);
      if (len >= 1 && *data == 0x42) {
        // Put device into wireless OTA state
        printf("OTA Reboot!\n");
        sleep_ms(50);
        picowota_reboot(true);
      }
    } else {
      printf("mqtt_incoming_data_cb: Ignoring payload...\n");
    }
  } else {
    // TODO: Handle fragmented payload if necessary
  }
}

/**** Connection and Subscriptions ****/

/* Callback for subscription request */
static void mqtt_sub_request_cb(void *arg, err_t result) {
  (void)arg;

  printf("Subscribe result: %d\n", result);
}

/* Helper function to subscribe and check for errors */
static void mqtt_subscribe_error_check(mqtt_client_t *client, const char *topic, u8_t qos,
                                       mqtt_request_cb_t cb, void *arg) {
  cyw43_arch_lwip_begin();
  err_t err = mqtt_subscribe(client, topic, qos, cb, arg);
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    printf("mqtt_subscribe return: %d\n", err);
  }
}

/* Callback for MQTT connection */
static void mqtt_connection_cb(mqtt_client_t *client, void *params,
                               mqtt_connection_status_t status) {
  if (status == MQTT_CONNECT_ACCEPTED) {
    printf("mqtt_connection_cb: Successfully connected\n");

    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, params);

    /* Subscribe to all topics */
    char topic[128] = {0};
    snprintf(topic, sizeof(topic), "%s/devices/%d/command/#", DANCING_DUCK_SUBSCRIPTION,
             DUCK_ID_NUM);
    mqtt_subscribe_error_check(client, topic, 1, mqtt_sub_request_cb, NULL);
    snprintf(topic, sizeof(topic), "%s/all_devices/command/#", DANCING_DUCK_SUBSCRIPTION);
    mqtt_subscribe_error_check(client, topic, 1, mqtt_sub_request_cb, NULL);

  } else {
    printf("mqtt_connection_cb: Disconnected, reason: %d\n", status);

    /* Its more nice to be connected, so try to reconnect */
    mqtt_connect(client, params);
  }
}

/* Function to connect to MQTT broker */
err_t mqtt_connect(mqtt_client_t *client, void *params) {
  struct mqtt_connect_client_info_t ci = {0};
  char buffer[16] = {0};
  snprintf(buffer, sizeof(buffer), "lwip_duck_%d", DUCK_ID_NUM);
  ci.client_id = buffer;

  ip_addr_t ip_addr;
  IP4_ADDR(&ip_addr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);

  printf("Connecting to MQTT Broker\n");
  cyw43_arch_lwip_begin();
  err_t err = mqtt_client_connect(client, &ip_addr, MQTT_PORT, mqtt_connection_cb, params, &ci);
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    printf("mqtt_connect return %d\n", err);
  } else {
    printf("Connected to MQTT Broker!\n");
  }

  return err;
}
