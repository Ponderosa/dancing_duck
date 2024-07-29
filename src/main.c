#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "adc.h"
#include "blink.h"
#include "commanding.h"
#include "config.h"
#include "hardware/watchdog.h"
#include "magnetometer.h"
#include "motor.h"
#include "mqtt.h"
#include "picowota/reboot.h"
#include "publish.h"
#include "task.h"
#include "watchdog.h"
#include "wifi.h"

#define PRINTF_DEBUG 1

char global_mac_address[32];

static void vTaskListInfo();

static mqtt_client_t static_client;

static void wifi_connect() {
  printf("Connecting to Wi-Fi...\n");
#ifdef PRINT_WIFI_CREDS
  printf("WiFi SSID: %s\n", WIFI_SSID);
  printf("WiFI Password: %s\n", WIFI_PASSWORD);
#endif

  bool connected = false;

  for (int i = 0; i < 3; i++) {
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK,
                                           WIFI_TIMEOUT_MS)) {
      printf("failed to connect to wifi.\n");
      watchdog_update();
    } else {
      printf("Connected.\n");
      connected = true;
      break;
    }
  }
  if (!connected) {
    // Todo: Schedule reboot
    picowota_reboot(false);
  }
}

void vInit() {
  // WiFi chip init - Must be ran in FreeRTOS Task
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  } else {
    printf("Wi-Fi init passed!\n");
    // Todo: Schedule reset
  }

  // Init task must manage watchdog
  watchdog_update();

  cyw43_arch_enable_sta_mode();

  switch (WIFI_MODE) {
    case MQTT:
      wifi_connect();
      break;
    case PING:
      wifi_connect();
      xTaskCreate(vPing, "Ping Task", 2048, NULL, 3, NULL);
      vTaskDelete(NULL);  // Delete the current task ! EARLY EXIT !
      break;
    case SCAN:
      xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);
      vTaskDelete(NULL);  // Delete the current task ! EARLY EXIT !
      break;
    default:
      printf("Error: WIFI_MODE misconfigured!");
      vTaskDelete(NULL);  // Delete the current task ! EARLY EXIT !
  }

  // Init task must manage watchdog
  watchdog_update();

  // Print MAC address
  uint8_t mac[6];
  cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
  snprintf(global_mac_address, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
  printf("MAC: %s\n", global_mac_address);

  // FreeRTOS Shared Resources
  QueueHandle_t motorQueue = xQueueCreate(MOTOR_QUEUE_DEPTH, sizeof(struct motorCommand));
  if (!motorQueue) {
    printf("Motor Queue Creation failed!\n");
  }

  SemaphoreHandle_t motorStop = xSemaphoreCreateBinary();
  if (!motorStop) {
    printf("Motor Semaphore Creation failed\n");
  }

  QueueHandle_t magMailbox = xQueueCreate(1, sizeof(struct magXYZ));
  if (!magMailbox) {
    printf("Motor Queue Creation failed!\n");
  }

  struct motorTaskParameters *motor_params =
      (struct motorTaskParameters *)pvPortMalloc(sizeof(struct motorTaskParameters));
  motor_params->command_queue = motorQueue;
  motor_params->mag_queue = magMailbox;
  motor_params->motor_stop = motorStop;

  struct publishTaskParameters *publish_params =
      (struct publishTaskParameters *)pvPortMalloc(sizeof(struct publishTaskParameters));
  publish_params->client = &static_client;
  publish_params->mag = magMailbox;

  struct mqttParameters *mqtt_params =
      (struct mqttParameters *)pvPortMalloc(sizeof(struct mqttParameters));
  mqtt_params->motor_queue = motorQueue;
  mqtt_params->motor_stop = motorStop;

  // FreeRTOS Task Creation - Lower number is lower priority!
  xTaskCreate(vBlinkTask, "Blink Task", 256, NULL, 1, NULL);
  xTaskCreate(vMagnetometerTask, "Mag Task", 512, (void *)magMailbox, 10, NULL);
  xTaskCreate(vMotorTask, "Motor Task", 1024, (void *)motor_params, 11, NULL);
  if (mqtt_connect(&static_client, (void *)mqtt_params) == ERR_OK) {
    xTaskCreate(vPublishTask, "MQTT Pub Task", 1024, (void *)publish_params, 3, NULL);
  }
  if (PRINTF_DEBUG) {
    xTaskCreate(vTaskListInfo, "Status Task", 512, NULL, 2, NULL);
  }
  xTaskCreate(vWatchDogTask, "Watchdog Task", 128, NULL, 1, NULL);

  printf("Init Complete\n");

  // Delete the current task
  vTaskDelete(NULL);
}

int main() {
  // Init PICO SDK - Currently Inits UART to GP0/GP1
  stdio_init_all();

  // Check Watchdog
  if (watchdog_caused_reboot()) {
    printf("\n>>> Rebooted by Watchdog!\n");
  } else {
    printf("\n>>> Clean boot\n");
  }

  // Enable Watchdog
  watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
  printf("Watchdog enabled: %ums\n", WATCHDOG_TIMEOUT_MS);
  watchdog_hw->scratch[5] = 0;
  watchdog_hw->scratch[6] = 0;

  // Enable ADC
  adcInit();

  // Announce Init
  printf("Dancing Duck Initialized\n");
  printf("Dancing Duck ID = %u\n", DUCK_ID_NUM);

  // Create init task (above), watchdog task, and kick off scheduler
  xTaskCreate(vInit, "Init Task", 2048, NULL, 1, NULL);
  vTaskStartScheduler();
}

//////////////// Temporary Status Task ////////////////

// Function to print the name, current state, priority, stack, and task number
// of all tasks
void vTaskListInfo() {
  UBaseType_t uxArraySize;
  TaskStatus_t *pxTaskStatusArray;

  vTaskDelay(1000);

  for (;;) {
    // Get the current free heap size
    size_t currentHeapSize = xPortGetFreeHeapSize();

    // Get the minimum free heap size ever
    size_t minimumHeapSize = xPortGetMinimumEverFreeHeapSize();

    printf("\n");

    // Print the heap sizes
    printf("Heap Info:\n");
    printf("Maximum Free: %u bytes\n", configTOTAL_HEAP_SIZE);
    printf("Current Free: %u bytes\n", (unsigned int)currentHeapSize);
    printf("Minimum Free: %u bytes\n", (unsigned int)minimumHeapSize);
    printf("Max %% Used  : %.2f%%\n",
           100.0 * (1.0 - (float)minimumHeapSize / (float)configTOTAL_HEAP_SIZE));

    // Get the number of tasks
    uxArraySize = uxTaskGetNumberOfTasks();

    // Allocate memory to hold the task information
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
      // Get the current task state
      uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);

      printf("\n");

      // Print the header
      printf("%-20s %-10s %-10s %-20s %-10s\n", "Task Name", "State", "Priority",
             "Stack Low Water Mark", "Task Number");

      // Print task information
      for (UBaseType_t i = 0; i < uxArraySize; i++) {
        printf("%-20s %-10u %-10lu %-20lu %-10lu\n", pxTaskStatusArray[i].pcTaskName,
               pxTaskStatusArray[i].eCurrentState, pxTaskStatusArray[i].uxCurrentPriority,
               pxTaskStatusArray[i].usStackHighWaterMark, pxTaskStatusArray[i].xTaskNumber);
      }

      printf("\n");

      // Free the allocated memory
      vPortFree(pxTaskStatusArray);
    } else {
      printf("Memory allocation failed.\n");
    }

    vTaskDelay(15000);
  }
}