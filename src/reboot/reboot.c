#include <inttypes.h>

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "config.h"
#include "hardware/structs/vreg_and_chip_reset.h"
#include "hardware/watchdog.h"
#include "picowota/reboot.h"
#include "reboot.h"

static volatile uint32_t *magic_number = &watchdog_hw->scratch[0];
static volatile uint32_t *boot_count = &watchdog_hw->scratch[1];
static volatile enum rebootReasonSoft *soft_reason =
    (enum rebootReasonSoft *)(&watchdog_hw->scratch[2]);
static enum rebootReasonHard hard_reason;
static enum rebootReasonSoft soft_reason_runtime;

void on_boot() {
  if (DD_MAGIC_NUM != *magic_number) {
    *magic_number = DD_MAGIC_NUM;
    *boot_count = 0;
    *soft_reason = NO_SOFT_REASON;
    watchdog_hw->scratch[3] = 0;  // Mag x cal
    watchdog_hw->scratch[7] = 0;  // Mag y cal
  }

  (*boot_count)++;

  printf("\n\n");
  if (watchdog_caused_reboot()) {
    printf(">>> Rebooted by Watchdog!\n");
    hard_reason = WATCHDOG_REASON;
  } else if (vreg_and_chip_reset_hw->chip_reset) {
    if (vreg_and_chip_reset_hw->chip_reset == 0x00000100) {
      hard_reason = HAD_POR;
    } else if (vreg_and_chip_reset_hw->chip_reset == 0x00010000) {
      hard_reason = HAD_RUN;
    } else {
      hard_reason = HAD_OTHER;
    }
  } else {
    printf(">>> Clean boot\n");
    hard_reason = NO_HARD_REASON;
  }

  printf("Soft Reason: %d\n", *soft_reason);
  printf("Boot Count: %ld\n", *boot_count);

  soft_reason_runtime = *soft_reason;
  *soft_reason = NO_SOFT_REASON;

  // Todo: consider adding other hard reasons (See table 191 in datasheet)
}

void reboot(enum rebootReasonSoft reason) {
  printf("Resetting! Reason: %d\n", reason);
  *soft_reason = reason;
  sleep_ms(100);
  picowota_reboot(false);  // Reboot into app
}

enum rebootReasonSoft rebootReasonSoft() { return soft_reason_runtime; }

enum rebootReasonHard rebootReasonHard() { return hard_reason; }

// Returns bootcount since power applied
uint32_t bootCount() { return *boot_count; }