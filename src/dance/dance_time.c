#include <inttypes.h>
#include <stdlib.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "dance_generator.h"
#include "stdint.h"

static const bool DEBUG_PRINT = true;
static const uint32_t TIME_INTERVAL_MS = 1000;
// static const uint32_t TIME_WINDOW_MS = TIME_INTERVAL_MS * 2;
static const int RETRY_COUNT = 5;

static uint32_t tick_count_last_update = 0;
static uint32_t server_time_last_update_ms = 0;
static bool server_time_set = false;

struct CurrentTime {
  uint32_t tick_count_last;
  uint32_t server_time_last_ms;
  uint32_t current_time_ms;
  uint32_t mod_time_ms;
  bool stable;
};

static void get_current_time_ms(struct CurrentTime *ct) {
  // Get shared time - Ensure coherency
  ct->tick_count_last = tick_count_last_update;
  ct->server_time_last_ms = server_time_last_update_ms;

  ct->stable = false;
  for (int i = 0; i < RETRY_COUNT; i++) {
    if ((ct->tick_count_last != tick_count_last_update) ||
        (ct->server_time_last_ms != server_time_last_update_ms)) {
      ct->tick_count_last = tick_count_last_update;
      ct->server_time_last_ms = server_time_last_update_ms;
    } else {
      ct->stable = true;
      break;
    }
  }

  // Calculate current time
  ct->current_time_ms = ct->server_time_last_ms + (xTaskGetTickCount() - ct->tick_count_last);
  ct->mod_time_ms = ct->current_time_ms % TIME_INTERVAL_MS;
}

static bool check_half_interval_window(struct CurrentTime *ct) {
  if (server_time_set && ct->stable) {
    uint32_t time_delta_ms = abs((int)ct->mod_time_ms - (int)(TIME_INTERVAL_MS / 2));
    if (time_delta_ms < TIME_INTERVAL_MS / 4) {
      return true;
    }
  }
  return false;
}

static uint32_t calculate_sleep_ticks(struct CurrentTime *ct) {
  uint32_t wake_time_ms = ct->current_time_ms - ct->mod_time_ms + TIME_INTERVAL_MS * 3 / 2;
  uint32_t wake_tick = wake_time_ms - ct->server_time_last_ms + ct->tick_count_last;
  if (DEBUG_PRINT) {
    printf("Sleep Ticks: %" PRIu32 "\n", wake_tick - xTaskGetTickCount());
  }
  return wake_tick - xTaskGetTickCount();
}

// Must be called from FreeRTOS task
void set_dance_server_time_ms(uint32_t time_ms) {
  tick_count_last_update = xTaskGetTickCount();
  server_time_last_update_ms = time_ms;
  server_time_set = true;
  if (DEBUG_PRINT) {
    printf("Server Time %" PRIu32 "ms\n", time_ms);
  }
}

void reset_dance_time() {
  tick_count_last_update = 0;
  server_time_last_update_ms = 0;
  server_time_set = false;
}

// Call Dance Generator Periodically
// Ensure we run this task once per second in the middle of the second
// i.e. at 0.5, 1.5, 2.5 seconds server time
// Assumes tick is millisecond based
void vDanceTimeTask(void *pvParameters) {
  init_dance_program();

  for (;;) {
    struct CurrentTime ct;
    get_current_time_ms(&ct);

    // Check if we are in valid half interval window
    if (check_half_interval_window(&ct)) {
      dance_generator((QueueHandle_t)pvParameters, ct.current_time_ms / TIME_INTERVAL_MS);
    }

    vTaskDelay(calculate_sleep_ticks(&ct));
  }
}
