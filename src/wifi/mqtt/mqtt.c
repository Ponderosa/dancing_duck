#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "mqtt.h"
#include "task.h"

#define IP_ADDR0                   (MQTT_BROKER_IP_A)
#define IP_ADDR1                   (MQTT_BROKER_IP_B)
#define IP_ADDR2                   (MQTT_BROKER_IP_C)
#define IP_ADDR3                   (MQTT_BROKER_IP_D)

/* Messages from duck to commander */
#define KEEP_ALIVE_SUBSCRIPTION    ("KeepAlive")
#define STANDARD_OUT_SUBSCRIPTION  ("StandardOut")
#define ERROR_SUBSCRIPTION         ("ErrorOut")

/* Messaages from commander to duck */
#define DUCK_COMMAND_SUBSCRIPTION  ("DuckCommand")

/* Used for MQTT to UART testing */
#define PRINT_PAYLOAD_SUBSCRIPTION ("PrintPayload")

/**** Incoming Messages ****/

/* Global variable to store the incoming publish ID */
static int inpub_id;

/* Callback for incoming publish */
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  printf("Incoming publish at topic %s with total length %u\n", topic, (unsigned int)tot_len);

  if (strcmp(topic, PRINT_PAYLOAD_SUBSCRIPTION) == 0) {
    inpub_id = 0;
  } else if (strcmp(topic, DUCK_COMMAND_SUBSCRIPTION) == 0) {
    inpub_id = 1;
  } else {
    inpub_id = 2;
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
      printf("Duck Command Received\n");
      enqueue_motor_command(arg, data, len);
    } else {
      printf("mqtt_incoming_data_cb: Ignoring payload...\n");
    }
  } else {
    // TODO: Handle fragmented payload if necessary
  }
}

/**** Publishing ****/

/* Callback for publish request */
static void mqtt_pub_request_cb(void *arg, err_t result) {
  if (result != ERR_OK) {
    printf("Publish result: %d\n", result);
  }
}

/* Task to publish status periodically */
void vMqttPublishStatus(void *pvParameters) {
  mqtt_client_t *client = (mqtt_client_t *)pvParameters;
  const char *subscription = KEEP_ALIVE_SUBSCRIPTION;

  // Get 64bit Unique ID
  char id[9];  // 8 bytes plus null terminator
  pico_get_unique_board_id_string(id, sizeof(id));
  printf("64-bit NAND FLASH ID: ");
  for (size_t i = 0; i < sizeof(id); i++) {
    printf("%02x ", (unsigned char)id[i]);
  }
  printf("\n");

  // Create Payload
  char pub_payload[64] = "Quack! - ";
  strcat(pub_payload, id);

  while (true) {
    err_t err = mqtt_publish(client, subscription, pub_payload, strlen(pub_payload), 1, 0,
                             mqtt_pub_request_cb, NULL);
    if (err != ERR_OK) {
      printf("Publish err: %d\n", err);
    }
    vTaskDelay(5000);
  }
}

/**** Connection and Subscriptions ****/

/* Callback for subscription request */
static void mqtt_sub_request_cb(void *arg, err_t result) {
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

    /* Subscribe to a topics */
    mqtt_subscribe_error_check(client, DUCK_COMMAND_SUBSCRIPTION, 1, mqtt_sub_request_cb, NULL);
    mqtt_subscribe_error_check(client, PRINT_PAYLOAD_SUBSCRIPTION, 1, mqtt_sub_request_cb, NULL);

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
  IP4_ADDR(&ip_addr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);

  printf("Connecting to MQTT Broker\n");
  err_t err = mqtt_client_connect(client, &ip_addr, MQTT_PORT, mqtt_connection_cb, arg, &ci);
  if (err != ERR_OK) {
    printf("mqtt_connect return %d\n", err);
  } else {
    printf("Connected to MQTT Broker!\n");
  }

  return err;
}
