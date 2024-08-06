#include <string.h>

#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "config.h"
#include "lis2mdl.h"
#include "magnetometer.h"
#include "math.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

static struct CircleCenter calibration_offset_checked;
static struct CircleCenter calibration_offset_raw;

// Find center of x, y circle to create calibration offset for magnetometer
// Kasa method chosen for highly efficient compute
// DOI: 10.1109/TIM.1976.6312298
// Created with help from Claude by Anthropic
static int find_circle_center_kasa_method(const double* x, const double* y, int size,
                                          struct CircleCenter* result) {
  if (size < 3) {
    return -1;  // Not enough points to define a circle
  }

  double x_m = 0, y_m = 0;
  double Suv = 0, Suu = 0, Svv = 0, Suuv = 0, Suvv = 0, Suuu = 0, Svvv = 0;

  // Compute means and sums
  for (int i = 0; i < size; i++) {
    x_m += x[i];
    y_m += y[i];
  }
  x_m /= size;
  y_m /= size;

  for (int i = 0; i < size; i++) {
    double u = x[i] - x_m;
    double v = y[i] - y_m;

    Suv += u * v;
    Suu += u * u;
    Svv += v * v;
    Suuv += u * u * v;
    Suvv += u * v * v;
    Suuu += u * u * u;
    Svvv += v * v * v;
  }

  // Solve the linear system
  double A[2][2] = {{Suu, Suv}, {Suv, Svv}};
  double B[2] = {(Suuu + Suvv) / 2.0, (Svvv + Suuv) / 2.0};

  // Check for division by zero
  double det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
  if (fabs(det) < EPSILON) {
    return -1;  // Division by zero detected
  }

  double uc = (A[1][1] * B[0] - A[0][1] * B[1]) / det;
  double vc = (-A[1][0] * B[0] + A[0][0] * B[1]) / det;

  // Compute center
  result->center_x = uc + x_m;
  result->center_y = vc + y_m;

  // Compute radius and RMSE
  double sum_squared_residuals = 0;
  double R_squared = uc * uc + vc * vc + (Suu + Svv) / size;

  // Check for negative radius (shouldn't happen, but just in case)
  if (R_squared < 0) {
    return -1;
  }

  double R = sqrt(R_squared);

  for (int i = 0; i < size; i++) {
    double dx = x[i] - result->center_x;
    double dy = y[i] - result->center_y;
    double distance_squared = dx * dx + dy * dy;

    // Check for division by zero (point exactly at center)
    if (distance_squared < EPSILON) {
      return -1;
    }

    double residual = sqrt(distance_squared) - R;
    sum_squared_residuals += residual * residual;
  }

  result->rmse = sqrt(sum_squared_residuals / size);

  return 0;  // Success
}

// Used for logging
void get_kasa_raw(struct CircleCenter* cr_out) {
  cr_out->center_x = calibration_offset_raw.center_x;
  cr_out->center_y = calibration_offset_raw.center_y;
  cr_out->rmse = calibration_offset_raw.rmse;
}

double get_heading(const struct MagXYZ* mag) {
  // Invert X reading due to placement of sensor
  double heading = atan2l((double)mag->y_uT, -(double)mag->x_uT);

  heading *= 180.0 / (double)M_PI;

  // Normalize to 0-360 degrees
  if (heading < 0) {
    heading += 360.0;
  }

  return heading;
}

void apply_calibration_kasa(struct MagXYZ* mag) {
  mag->x_uT -= calibration_offset_checked.center_x;
  mag->y_uT -= calibration_offset_checked.center_y;
}

void run_calibration(double* x_vals_uT, double* y_vals_uT, struct MagXYZ* mag,
                     SemaphoreHandle_t calibrate) {
  static size_t counter = 0;

  // Fill X,Y Buffers
  x_vals_uT[counter] = mag->x_uT;
  y_vals_uT[counter] = mag->y_uT;
  counter++;

  // Check for end of calibration
  if (counter >= KASA_ARRAY_DEPTH) {
    counter = 0;
    // Drop Semaphore to 0
    if (xSemaphoreTake(calibrate, 0) == pdFALSE) {
      printf("Error: Calibrate Semaphore");
    }
  }

  // Calibration
  if (counter % KASA_LOOP_COUNTER == 0) {
    struct CircleCenter cr;
    memset(&cr, 0, sizeof(struct CircleCenter));
    // Find circle with existing data
    if (find_circle_center_kasa_method(x_vals_uT, y_vals_uT, counter, &cr)) {
      printf("KASA Divide by Zero detected\n");
    } else if (cr.rmse > KASA_RMSE_ACCEPTABLE_LIMIT) {
      calibration_offset_checked = cr;
      calibration_offset_raw = cr;
    } else {
      calibration_offset_raw = cr;
    }
  }
}

void vMagnetometerTask(void* pvParameters) {
  struct MagnetometerTaskParameters* mtp = (struct MagnetometerTaskParameters*)pvParameters;

  if (lis2_init() == false) {
    printf("Magnetometer Init Failed!\n");
  }

  memset(&calibration_offset_checked, 0, sizeof(struct CircleCenter));
  memset(&calibration_offset_raw, 0, sizeof(struct CircleCenter));

  double x_vals_uT[KASA_ARRAY_DEPTH];
  double y_vals_uT[KASA_ARRAY_DEPTH];
  memset(&x_vals_uT, 0, sizeof(double) * KASA_ARRAY_DEPTH);
  memset(&y_vals_uT, 0, sizeof(double) * KASA_ARRAY_DEPTH);

  for (;;) {
    // Done first in the loop to prevent kasa algorithm from adding jitter
    struct MagXYZ mag = get_xyz_uT();
    xQueueOverwrite(mtp->mag_mailbox, &mag);

    if (uxSemaphoreGetCount(mtp->calibrate)) {
      run_calibration(x_vals_uT, y_vals_uT, &mag, mtp->calibrate);
    }

    vTaskDelay(100);
  }
}