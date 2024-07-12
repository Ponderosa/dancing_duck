
#include <stdio.h>

#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"

#define BATTERY_GPIO 26
#define BATTERY_ADC  0
#define TEMP_ADC     4

// 12-bit conversion, assume max value == ADC_VREF == 3.3 V
static const float conversion_factor = 3.3f / (1 << 12);

void adcInit() {
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_gpio_init(26);
}

float getBattery_V() {
  adc_select_input(BATTERY_ADC);

  float adc_voltage = (float)adc_read() * conversion_factor;

  return adc_voltage;
  return adc_voltage * 5.0 / 3.0;
}

float getTemp_C() {
  adc_select_input(TEMP_ADC);

  float adc_voltage = (float)adc_read() * conversion_factor;
  return 27.0 - (adc_voltage - 0.706) / 0.001721;
}
