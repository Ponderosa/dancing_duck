#include <string.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "commanding.h"
#include "mqtt.h"
#include "picowota/reboot.h"
#include "task.h"

#define IP_ADDR0     (MQTT_BROKER_IP_A)
#define IP_ADDR1     (MQTT_BROKER_IP_B)
#define IP_ADDR2     (MQTT_BROKER_IP_C)
#define IP_ADDR3     (MQTT_BROKER_IP_D)

#define IP_ADDR0_ALT (MQTT_BROKER_IP_A_ALT)
#define IP_ADDR1_ALT (MQTT_BROKER_IP_B_ALT)
#define IP_ADDR2_ALT (MQTT_BROKER_IP_C_ALT)
#define IP_ADDR3_ALT (MQTT_BROKER_IP_D_ALT)

extern bool global_alt;

/**** Incoming Messages ****/
#define BUFFER_SIZE 128

int strcmp_formatted(const char *topic, const char *format, ...) {
  char formatted[BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(formatted, BUFFER_SIZE, format, args);
  va_end(args);
  return strcmp(topic, formatted);
}

/* Global variable to store the incoming publish ID */
static int inpub_id;

/* Callback for incoming publish */
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  (void)arg;

  printf("Incoming publish at topic %s with total length %u\n", topic, (unsigned int)tot_len);

  if (strcmp_formatted(topic, "%s/all_devices/command/uart_tx", DANCING_DUCK_SUBSCRIPTION) == 0) {
    inpub_id = 0;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/motor", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 1;
  } else if (strcmp_formatted(topic, "%s/devices/%d/command/bootloader", DANCING_DUCK_SUBSCRIPTION,
                              DUCK_ID_NUM) == 0) {
    inpub_id = 2;
  } else {
    inpub_id = 3;
  }
}

/* Callback for incoming data */
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
  printf("Incoming publish payload with length %d, flags %u\n", len, (unsigned int)flags);

  if (flags & MQTT_DATA_FLAG_LAST) {
    if (inpub_id == 0) {
      if (data[len - 1] == 0) {
        printf("mqtt_incoming_data_cb: %s\n", (const char *)data);
      } else {
        printf("Termination check failed \n");
      }
    } else if (inpub_id == 1) {
      printf("Motor Command Received\n");
      enqueue_motor_command(arg, (char *)data, len);
    } else if (inpub_id == 2) {
      printf("Bootloader Command Received\n");
      printf("Data: %u", *data);
      if (len >= 1 && *data == 0x42) {
        // Put device into wireless OTA state
        printf("Reboot Now!\n");
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
  err_t err = mqtt_subscribe(client, topic, qos, cb, arg);
  if (err != ERR_OK) {
    printf("mqtt_subscribe return: %d\n", err);
  }
}

/* Callback for MQTT connection */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  if (status == MQTT_CONNECT_ACCEPTED) {
    printf("mqtt_connection_cb: Successfully connected\n");

    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);

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
    mqtt_connect(client, arg);
  }
}

/* Function to connect to MQTT broker */
err_t mqtt_connect(mqtt_client_t *client, void *arg) {
  struct mqtt_connect_client_info_t ci = {0};
  ci.client_id = "lwip_test";

  ip_addr_t ip_addr;
  if (global_alt) {
    IP4_ADDR(&ip_addr, IP_ADDR0_ALT, IP_ADDR1_ALT, IP_ADDR2_ALT, IP_ADDR3_ALT);
  } else {
    IP4_ADDR(&ip_addr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
  }

  printf("Connecting to MQTT Broker\n");
  err_t err = mqtt_client_connect(client, &ip_addr, MQTT_PORT, mqtt_connection_cb, arg, &ci);
  if (err != ERR_OK) {
    printf("mqtt_connect return %d\n", err);
  } else {
    printf("Connected to MQTT Broker!\n");
  }

  return err;
}
