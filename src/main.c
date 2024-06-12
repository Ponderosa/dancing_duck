#include "FreeRTOS.h"
#include "blink.h"
// #include "mqtt_agent.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"
#include "wifi.h"

#define WIFI_SCAN (0)
#define MQTT_TEST (0)

mqtt_client_t static_client;

/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result) {
  if (result != ERR_OK) {
    printf("Publish result: %d\n", result);
  }
}

static void example_publish(mqtt_client_t *client, void *arg) {
  const char *pub_payload = "PubSubHubLubJub";
  err_t err;
  u8_t qos = 2;    /* 0 1 or 2, see MQTT specification */
  u8_t retain = 0; /* No don't retain such crappy payload... */
  err = mqtt_publish(client, "duck_dance_1", pub_payload, strlen(pub_payload),
                     qos, retain, mqtt_pub_request_cb, arg);
  if (err != ERR_OK) {
    printf("Publish err: %d\n", err);
  }
}

void vInit() {
  // WiFi chip init - Must be ran in FreeRTOS Task
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  } else {
    printf("Wi-Fi init passed!\n");
  }

  cyw43_arch_enable_sta_mode();

  if (!WIFI_SCAN) {
    printf("Connecting to Wi-Fi...\n");
    printf("WiFi SSID: %s\n", WIFI_SSID);
    printf("WiFI Password: %s\n", WIFI_PASSWORD);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
      printf("failed to connect.\n");
    } else {
      printf("Connected.\n");
    }
  }

  // FreeRTOS Task Creation
  // All task creation should remain here so we can easily manipulate
  // stack size and priority in relationship to one another.
  // Lower number priority is lower priority!
  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  if (WIFI_SCAN) {
    xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);
  } else if (MQTT_TEST) {
    // vStartMQTTAgent();
  } else {
    // xTaskCreate(vPing, "Ping Task", 2048, NULL, 2, NULL);
  }

  example_do_connect(&static_client);

  vTaskDelay(1000);

  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);
  example_publish(&static_client, NULL);

  vTaskDelay(1000);

  example_publish(&static_client, NULL);

  for (;;) {
  }

  // Delete the current task
  vTaskDelete(NULL);
}

void main() {
  stdio_init_all();

  printf("Dancing Duck\n");

  xTaskCreate(vInit, "Init Task", 2048, NULL, 1, NULL);

  vTaskStartScheduler();
}