
#include <stdio.h>

#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"

static const uint32_t BATTERY_GPIO_NUM = 26;
static const uint32_t BATTERY_ADC_NUM = 0;
static const uint32_t TEMP_ADC_NUM = 4;

// 12-bit conversion, assume max value == ADC_VREF == 3.3 V
static const double adc_conversion_factor = 3.3 / (1 << 12);
static const double battery_voltage_divider_factor = 5.0 / 3.0;

void init_adc() {
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_gpio_init(BATTERY_GPIO_NUM);
}

double get_battery_V() {
  adc_select_input(BATTERY_ADC_NUM);

  double adc_voltage = (double)adc_read() * adc_conversion_factor;
  return adc_voltage * battery_voltage_divider_factor;
}

double get_temp_C() {
  adc_select_input(TEMP_ADC_NUM);

  double adc_voltage = (double)adc_read() * adc_conversion_factor;
  return 27.0 - (adc_voltage - 0.706) / 0.001721;
}
