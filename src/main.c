#include <inttypes.h>

#include "FreeRTOS.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "adc.h"
#include "blink.h"
#include "commanding.h"
#include "config.h"
#include "dance_time.h"
#include "hardware/watchdog.h"
#include "magnetometer.h"
#include "motor.h"
#include "mqtt.h"
#include "picowota/reboot.h"
#include "publish.h"
#include "reboot.h"
#include "task.h"
#include "watchdog.h"
#include "wifi.h"

// Local Variables
static const bool FREERTOS_PRINT_INFO_DEBUG = false;  // Debug only! Causes long scheduler holds
static mqtt_client_t static_client;

// Global Variables
char global_mac_address[32];

// Local Functions
static void print_task_information(const TaskStatus_t *task_status_array, size_t array_size) {
  printf("\n");
  printf("%-20s %-10s %-10s %-20s %-10s\n", "Task Name", "State", "Priority",
         "Stack Low Water Mark", "Task Number");

  for (UBaseType_t i = 0; i < array_size; i++) {
    printf("%-20s %-10" PRIu32 " %-10" PRIu32 " %-20" PRIu32 " %-10" PRIu32 "\n",
           task_status_array[i].pcTaskName, (uint32_t)task_status_array[i].eCurrentState,
           task_status_array[i].uxCurrentPriority, task_status_array[i].usStackHighWaterMark,
           task_status_array[i].xTaskNumber);
  }
}

static void print_heap_information() {
  size_t minimum_heap_size = xPortGetMinimumEverFreeHeapSize();

  printf("\n");
  printf("Heap Info:\n");
  printf("Maximum Free: %" PRIu32 " bytes\n", (uint32_t)configTOTAL_HEAP_SIZE);
  printf("Current Free: %" PRIu32 " bytes\n", (uint32_t)xPortGetFreeHeapSize());
  printf("Minimum Free: %" PRIu32 " bytes\n", (uint32_t)minimum_heap_size);
  printf("Max %% Used  : %.2f%%\n",
         100.0 * (1.0 - (double)minimum_heap_size / (double)configTOTAL_HEAP_SIZE));
}

// Print FreeRTOS stats periodically - Debug Only
static void vFreeRTOSInfoTask() {
  UBaseType_t uxArraySize;
  TaskStatus_t *pxTaskStatusArray;

  vTaskDelay(1000);

  for (;;) {
    print_heap_information();

    uxArraySize = uxTaskGetNumberOfTasks();
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
      uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);
      print_task_information(pxTaskStatusArray, uxArraySize);

      vPortFree(pxTaskStatusArray);
    } else {
      printf("Memory allocation failed.\n");
    }

    vTaskDelay(15000);
  }
}

static void wifi_connect() {
  printf("Connecting to Wi-Fi...\n");
  if (PRINT_WIFI_CREDS) {
    printf("WiFi SSID: %s\n", WIFI_SSID);
    printf("WiFI Password: %s\n", WIFI_PASSWORD);
  }

  bool connected = false;

  for (int i = 0; i < WIFI_CONNECT_RETRY_COUNT; i++) {
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
    // Todo: consider return to dock
    reboot(WIFI_CONNECT_REASON);
  }
}

// This task as early exits! Be careful with allocation.
static void vInitTask() {
  // WiFi chip init - Must be ran in FreeRTOS Task
  if (cyw43_arch_init()) {
    printf("Wi-Fi init failed\n");
    // Todo: consider return to dock
    reboot(WIFI_INIT_REASON);
  } else {
    printf("Wi-Fi init passed!\n");
  }

  // Init task must manage watchdog
  watchdog_update();

  // Enable "Station" mode (as opposed to Access Point)
  cyw43_arch_enable_sta_mode();

  // Modes other than MQTT are for bringup and debug
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
  QueueHandle_t motor_queue = xQueueCreate(MOTOR_QUEUE_DEPTH, sizeof(struct MotorCommand));
  if (!motor_queue) {
    printf("Motor Queue Creation failed!\n");
  }

  QueueHandle_t duck_mode_mailbox = xQueueCreate(1, sizeof(enum DuckMode));
  if (!duck_mode_mailbox) {
    printf("Motor Queue Creation failed!\n");
  }

  QueueHandle_t mag_mailbox = xQueueCreate(1, sizeof(struct MagXYZ));
  if (!mag_mailbox) {
    printf("Motor Queue Creation failed!\n");
  }

  SemaphoreHandle_t motor_stop_semaphore = xSemaphoreCreateBinary();
  if (!motor_stop_semaphore) {
    printf("Motor Semaphore Creation failed!\n");
  }

  SemaphoreHandle_t calibration_semaphore = xSemaphoreCreateBinary();
  if (!calibration_semaphore) {
    printf("Calibration Semaphore Creation failed!\n");
  }

  // Set Duck Mode
  enum DuckMode dm = DRY_DOCK;
  if (calibration_data_found()) {
    dm = DANCE;
  }
  xQueueOverwrite(duck_mode_mailbox, &dm);

  // Assemble shared resource
  struct MagnetometerTaskParameters *mag_params =
      (struct MagnetometerTaskParameters *)pvPortMalloc(sizeof(struct MagnetometerTaskParameters));
  mag_params->mag_mailbox = mag_mailbox;
  mag_params->calibrate = calibration_semaphore;

  struct MotorTaskParameters *motor_params =
      (struct MotorTaskParameters *)pvPortMalloc(sizeof(struct MotorTaskParameters));
  motor_params->command_queue = motor_queue;
  motor_params->mag_queue = mag_mailbox;
  motor_params->motor_stop = motor_stop_semaphore;

  struct PublishTaskParameters *publish_params =
      (struct PublishTaskParameters *)pvPortMalloc(sizeof(struct PublishTaskParameters));
  publish_params->client = &static_client;
  publish_params->mag = mag_mailbox;
  publish_params->duck_mode_mailbox = duck_mode_mailbox;

  struct MqttParameters *mqtt_params =
      (struct MqttParameters *)pvPortMalloc(sizeof(struct MqttParameters));
  mqtt_params->motor_queue = motor_queue;
  mqtt_params->duck_mode_mailbox = duck_mode_mailbox;
  mqtt_params->motor_stop = motor_stop_semaphore;
  mqtt_params->calibrate = calibration_semaphore;

  struct DanceTimeParameters *dance_params =
      (struct DanceTimeParameters *)pvPortMalloc(sizeof(struct DanceTimeParameters));
  dance_params->motor_queue = motor_queue;
  dance_params->duck_mode_mailbox = duck_mode_mailbox;

  // FreeRTOS Task Creation - Lower number is lower priority!
  xTaskCreate(vBlinkTask, "Blink Task", 512, NULL, 1, NULL);
  xTaskCreate(vMagnetometerTask, "Mag Task", 2048, (void *)mag_params, 10, NULL);
  xTaskCreate(vMotorTask, "Motor Task", 512, (void *)motor_params, 11, NULL);
  xTaskCreate(vDanceTimeTask, "Dance Task", 512, (void *)dance_params, 12, NULL);
  if (mqtt_connect(&static_client, (void *)mqtt_params) == ERR_OK) {
    xTaskCreate(vPublishTask, "MQTT Pub Task", 1024, (void *)publish_params, 3, NULL);
  }
  if (FREERTOS_PRINT_INFO_DEBUG) {
    xTaskCreate(vFreeRTOSInfoTask, "Print Status Task", 512, NULL, 2, NULL);
  }
  TaskHandle_t xHandle;
  xTaskCreate(vWatchDogTask, "Watchdog Task", 128, NULL, 1, &(xHandle));
  if (DEBUG_IDLE) {
    // Toggle for CPU utilization on Logic Analyzer
    vTaskCoreAffinitySet(xHandle, 0x01);
    xTaskCreate(vToggleTask, "Toggle Task", 128, NULL, 1, &(xHandle));
    vTaskCoreAffinitySet(xHandle, 0x02);
  }

  /*
   * There are two hidden tasks in addition to the usual FreeRTOS tasks.
   * One is the pico async_context task, and the other is the tcp_ip task from LWIP.
   * The async_context_task parameters can be overridden in CMakeLists. Ex. CYW43_TASK_PRIORITY=21.
   * The LWIP task parameters can be updated in lwipopts.h. Ex. #define TCPIP_THREAD_PRIO 20
   * Note: Their are two IDLE tasks, one for each core. And only one timer service task.
   */

  printf("Init Task Complete\n");

  // Delete the current task
  vTaskDelete(NULL);
}

int main() {
  enable_watchdog();
  stdio_uart_init();

  // Init run pin override
  gpio_init(22);
  gpio_set_dir(22, GPIO_OUT);
  gpio_put(22, 1);

  init_adc();

  // Check boot reason and increment boot counter
  on_boot();

  printf("Duck ID = %" PRIu32 "\n", (uint32_t)DUCK_ID_NUM);

  // Wifi init must happen in task
  xTaskCreate(vInitTask, "Init Task", 2048, NULL, 1, NULL);
  vTaskStartScheduler();
}
