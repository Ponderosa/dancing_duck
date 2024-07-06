#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "blink.h"
#include "commanding.h"
#include "config.h"
#include "hardware/watchdog.h"
#include "magnetometer.h"
#include "motor.h"
#include "mqtt.h"
#include "task.h"
#include "watchdog.h"
#include "wifi.h"

static void vTaskListInfo();

static mqtt_client_t static_client;

static void wifi_connect() {
  printf("Connecting to Wi-Fi...\n");
#ifdef PRINT_WIFI_CREDS
  printf("WiFi SSID: %s\n", WIFI_SSID);
  printf("WiFI Password: %s\n", WIFI_PASSWORD);
#endif
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK,
                                         30000)) {
    printf("failed to connect.\n");
  } else {
    printf("Connected.\n");
  }
}

void vInit() {
  // WiFi chip init - Must be ran in FreeRTOS Task
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  } else {
    printf("Wi-Fi init passed!\n");
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

  // FreeRTOS Queue Creation
  QueueHandle_t motorQueue = xQueueCreate(MOTOR_QUEUE_DEPTH, sizeof(motorCommand_t));
  if (!motorQueue) {
    printf("Motor Queue Creation failed!\n");
  }

  QueueHandle_t magMailbox = xQueueCreate(1, sizeof(MagXYZ));
  if (!motorQueue) {
    printf("Motor Queue Creation failed!\n");
  }

  // FreeRTOS Task Creation
  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  xTaskCreate(vMagnetometerTask, "Mag", 2048, &magMailbox, 1, NULL);
  xTaskCreate(vMotorTask, "Motor Task", 2048, &motorQueue, 3, NULL);
  if (mqtt_connect(&static_client, &motorQueue) == ERR_OK) {
    xTaskCreate(vMqttPublishStatus, "MQTT Pub Task", 2048, (void *)(&static_client), 2, NULL);
  }
  xTaskCreate(vTaskListInfo, "Status Task", 2048, NULL, 2, NULL);

  // Init task must manage watchdog
  watchdog_update();

  // Delete the current task
  vTaskDelete(NULL);
}

int main() {
  // Init PICO SDK - Currently Inits UART to GP0/GP1
  stdio_init_all();

  // Check Watchdog
  if (watchdog_caused_reboot()) {
    printf("Rebooted by Watchdog!\n");
  } else {
    printf("Clean boot\n");
  }

  // Enable Watchdog
  watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);

  // Announce Init
  printf("Dancing Duck Initialized\n");

  // Create init task (above), watchdog task, and kick off scheduler
  xTaskCreate(vInit, "Init Task", 2048, NULL, 1, NULL);
  xTaskCreate(vWatchDogTask, "Watchdog Task", 2048, NULL, 1, NULL);  // Lowest Priority
  vTaskStartScheduler();
}

//////////////// Temporary Status Task ////////////////

// Function to print the name, current state, priority, stack, and task number
// of all tasks
void vTaskListInfo() {
  UBaseType_t uxArraySize;
  TaskStatus_t *pxTaskStatusArray;

  for (;;) {
    vTaskDelay(15000);

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
  }
}