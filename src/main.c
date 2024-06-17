#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "blink.h"
#include "commanding.h"
#include "motor.h"
#include "mqtt.h"
#include "task.h"
#include "wifi.h"

/* Creates EARLY EXIT! and disables other WiFi Features */
#define WIFI_SCAN_TEST (0)

/* Can be run concurrently with MQTT */
#define PING_TEST      (0)

/* Uncomment to print WiFi creds for debugging - Do not commit uncommented*/
// #define PRINT_WIFI_CREDS

void vTaskListInfo();

mqtt_client_t static_client;

void vInit() {
  // WiFi chip init - Must be ran in FreeRTOS Task
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
  } else {
    printf("Wi-Fi init passed!\n");
  }

  cyw43_arch_enable_sta_mode();

  if (!WIFI_SCAN_TEST) {
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

  // FreeRTOS Queue Creation
  QueueHandle_t motorQueue = xQueueCreate(16, sizeof(motorCommand_t));
  if (!motorQueue) {
    printf("Motor Queue Creation failed!\n");
  } else {
    printf("Motor Queue Created!\n");
  }

  // FreeRTOS Task Creation
  // All task creation should remain here so we can easily manipulate
  // stack size and priority in relationship to one another.
  // Lower number priority is lower priority!
  xTaskCreate(vBlinkTask, "Blink Task", 2048, NULL, 1, NULL);
  xTaskCreate(vMotorTask, "Motor Task", 2048, &motorQueue, 3, NULL);

  /////// WIFI SCAN ////////
  if (WIFI_SCAN_TEST) {
    xTaskCreate(vScanWifi, "Scan Wifi Task", 2048, NULL, 2, NULL);
    // Delete the current task ! EARLY EXIT !
    vTaskDelete(NULL);
  }

  /////// Normal Tasks ////////
  if (PING_TEST) {
    xTaskCreate(vPing, "Ping Task", 2048, NULL, 3, NULL);
  }
  // Start MQTT Pub/Sub
  if (mqtt_connect(&static_client, &motorQueue) == ERR_OK) {
    xTaskCreate(vMqttPublishStatus, "MQTT Pub Task", 2048, (void *)(&static_client), 3, NULL);
  }

  vTaskDelay(10000);
  xTaskCreate(vTaskListInfo, "Status Task", 2048, NULL, 2, NULL);

  // Delete the current task
  vTaskDelete(NULL);
}

void main() {
  // Init UART and announce boot
  stdio_init_all();
  printf("Dancing Duck\n");

  // Initialize WiFi and other Tasks
  xTaskCreate(vInit, "Init Task", 2048, NULL, 1, NULL);
  vTaskStartScheduler();
}

//////// Temporary Status Task /////////

// Function to print the name, current state, priority, stack, and task number
// of all tasks
void vTaskListInfo() {
  // The number of tasks currently being managed
  UBaseType_t uxArraySize;

  // Create an array to hold the task status information
  TaskStatus_t *pxTaskStatusArray;
  for (;;) {
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
        printf("%-20s %-10u %-10u %-20u %-10u\n", pxTaskStatusArray[i].pcTaskName,
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