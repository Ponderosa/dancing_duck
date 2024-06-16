#include "wifi.h"

#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"

/********** WiFi Scan **********/

#define WIFI_SCAN_INTERVAL_MS (10000)
#define WIFI_SCAN_TASK_INTERVAL_MS (1000)

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
  if (result) {
    printf(
        "ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x "
        "sec: %u\n",
        result->ssid, result->rssi, result->channel, result->bssid[0],
        result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4],
        result->bssid[5], result->auth_mode);
  }
  return 0;
}

void vScanWifi() {
  absolute_time_t scan_time = nil_time;
  bool scan_in_progress = false;
  for (;;) {
    if (absolute_time_diff_us(get_absolute_time(), scan_time) < 0) {
      if (!scan_in_progress) {
        cyw43_wifi_scan_options_t scan_options = {0};
        int err =
            cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
        if (err == 0) {
          printf("\nPerforming wifi scan\n");
          scan_in_progress = true;
        } else {
          printf("Failed to start scan: %d\n", err);
          scan_time = make_timeout_time_ms(WIFI_SCAN_INTERVAL_MS);
        }
      } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
        scan_time = make_timeout_time_ms(WIFI_SCAN_INTERVAL_MS);
        scan_in_progress = false;
      }
    }
    vTaskDelay(WIFI_SCAN_TASK_INTERVAL_MS);
  }
}

/********** Ping **********/

#include "ping.h"

#define PING_ADDR ("8.8.8.8")

void vPing() {
  ip_addr_t ping_addr;
  ipaddr_aton(PING_ADDR, &ping_addr);
  ping_init(&ping_addr);

  while (true) {
    vTaskDelay(100);
  }
}

/********** MQTT **********/

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "pico/unique_id.h"

#define IP_ADDR0 (MQTT_BROKER_IP_A)
#define IP_ADDR1 (MQTT_BROKER_IP_B)
#define IP_ADDR2 (MQTT_BROKER_IP_C)
#define IP_ADDR3 (MQTT_BROKER_IP_D)

/* The idea is to demultiplex topic and create some reference to be used in data
   callbacks Example here uses a global variable, better would be to use a
   member in arg If RAM and CPU budget allows it, the easiest implementation
   might be to just take a copy of the topic string and use it in
   mqtt_incoming_data_cb
*/
static int inpub_id;
static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len) {
  printf("Incoming publish at topic %s with total length %u\n", topic,
         (unsigned int)tot_len);

  /* Decode topic string into a user defined reference */
  if (strcmp(topic, PRINT_PAYLOAD_SUBSCRIPTION) == 0) {
    inpub_id = 0;
  } else if (strcmp(topic, DUCK_COMMAND_SUBSCRIPTION) == 0) {
    /* All topics starting with 'A' might be handled at the same way */
    inpub_id = 1;
  } else {
    /* For all other topics */
    inpub_id = 2;
  }
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
                                  u8_t flags) {
  printf("Incoming publish payload with length %d, flags %u\n", len,
         (unsigned int)flags);

  if (flags & MQTT_DATA_FLAG_LAST) {
    /* Last fragment of payload received (or whole part if payload fits receive
       buffer See MQTT_VAR_HEADER_BUFFER_LEN)  */

    /* Call function or do action depending on reference, in this case inpub_id
     */
    if (inpub_id == 0) {
      /* Don't trust the publisher, check zero termination */
      if (data[len - 1] == 0) {
        printf("mqtt_incoming_data_cb: %s\n", (const char *)data);
      } else {
        printf("Termination check failed \n");
      }
    } else if (inpub_id == 1) {
      printf("Duck Command Received\n");
    } else {
      printf("mqtt_incoming_data_cb: Ignoring payload...\n");
    }
  } else {
    /* Handle fragmented payload, store in buffer, write to file or whatever */
  }
}

static void mqtt_sub_request_cb(void *arg, err_t result) {
  /* Just print the result code here for simplicity,
     normal behaviour would be to take some action if subscribe fails like
     notifying user, retry subscribe or disconnect from server */
  printf("Subscribe result: %d\n", result);
}

static void mqtt_subscribe_error_check(mqtt_client_t *client, const char *topic,
                                       u8_t qos, mqtt_request_cb_t cb,
                                       void *arg) {
  err_t err = mqtt_subscribe(client, topic, qos, cb, arg);

  if (err != ERR_OK) {
    printf("mqtt_subscribe return: %d\n", err);
  }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                               mqtt_connection_status_t status) {
  if (status == MQTT_CONNECT_ACCEPTED) {
    printf("mqtt_connection_cb: Successfully connected\n");

    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb,
                            mqtt_incoming_data_cb, arg);

    /* Subscribe to a topics */
    mqtt_subscribe_error_check(client, DUCK_COMMAND_SUBSCRIPTION, 1,
                               mqtt_sub_request_cb, arg);
    mqtt_subscribe_error_check(client, PRINT_PAYLOAD_SUBSCRIPTION, 1,
                               mqtt_sub_request_cb, arg);

  } else {
    printf("mqtt_connection_cb: Disconnected, reason: %d\n", status);

    /* Its more nice to be connected, so try to reconnect */
    mqtt_connect(client);
  }
}

err_t mqtt_connect(mqtt_client_t *client) {
  struct mqtt_connect_client_info_t ci;
  err_t err;

  /* Setup an empty client info structure */
  memset(&ci, 0, sizeof(ci));

  /* Minimal amount of information required is client identifier, so set it here
   */
  ci.client_id = "lwip_test";

  /* IP_ADDR is used to initialize IP address format in lwIP. */
  ip_addr_t ip_addr;
  IP4_ADDR(&ip_addr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);

  /* Initiate client and connect to server, if this fails immediately an error
     code is returned otherwise mqtt_connection_cb will be called with
     connection result after attempting to establish a connection with the
     server. For now MQTT version 3.1.1 is always used */

  printf("Connecting to MQTT Broker\n");

  err = mqtt_client_connect(client, &ip_addr, MQTT_PORT, mqtt_connection_cb, 0,
                            &ci);

  printf("Connected to MQTT Broker!\n");

  /* For now just print the result code if something goes wrong */
  if (err != ERR_OK) {
    printf("mqtt_connect return %d\n", err);
  }

  return err;
}

/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result) {
  if (result != ERR_OK) {
    printf("Publish result: %d\n", result);
  }
}

void vMqttPublishStatus(void *pvParameters) {
  void *arg = NULL;
  mqtt_client_t *client = (mqtt_client_t *)pvParameters;
  const char *subscription = KEEP_ALIVE_SUBSCRIPTION;

  // Get 64bit Unique ID
  char id[9];  // 8 bytes plus null terminator
  pico_get_unique_board_id_string(id, 9);
  printf("64-bit NAND FLASH ID: ");
  for (int i = 0; i < 9; i++) {
    printf("%02x ", (unsigned char)id[i]);
  }
  printf("\n");

  // Create Payload
  char pub_payload[64] = "Quack! - ";
  strcat(pub_payload, id);

  while (true) {
    err_t err;
    u8_t qos = 1;    /* 0 1 or 2, see MQTT specification */
    u8_t retain = 0; /* No don't retain such crappy payload... */
    err = mqtt_publish(client, subscription, pub_payload, strlen(pub_payload),
                       qos, retain, mqtt_pub_request_cb, arg);
    if (err != ERR_OK) {
      printf("Publish err: %d\n", err);
    }
    vTaskDelay(5000);
  }
}