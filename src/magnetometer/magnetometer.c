#include "FreeRTOS.h"

#include "pico/printf.h"
#include "pico/stdlib.h"

#include "lis2mdl.h"
#include "magnetometer.h"
#include "math.h"
#include "queue.h"
#include "task.h"

static const size_t KASA_ARRAY_DEPTH = 250;
// Small value to check for near-zero conditions
static const double EPSILON = 1e-10;

static struct CircleResult calibration_offset = {0};

// Created using Claude by Anthropic
static int kasa_method(const double* x, const double* y, int size, struct CircleResult* result) {
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

void get_kasa(struct CircleResult* cr_out) {
  cr_out->center_x = calibration_offset.center_x;
  cr_out->center_y = calibration_offset.center_y;
  cr_out->rmse = calibration_offset.rmse;
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

void apply_kasa(struct MagXYZ* mag) {
  mag->x_uT -= calibration_offset.center_x;
  mag->y_uT -= calibration_offset.center_y;
}

void vMagnetometerTask(void* pvParameters) {
  QueueHandle_t mailbox = (QueueHandle_t)pvParameters;
  if (lis2_init() == false) {
    printf("Magnetometer Init Failed!\n");
  }

  double x_vals_uT[KASA_ARRAY_DEPTH];
  double y_vals_uT[KASA_ARRAY_DEPTH];
  size_t counter = 0;

  for (;;) {
    struct MagXYZ mag = get_xyz_uT();
    xQueueOverwrite(mailbox, &mag);

    x_vals_uT[counter] = mag.x_uT;
    y_vals_uT[counter] = mag.y_uT;
    counter++;
    if (counter >= KASA_ARRAY_DEPTH) {
      counter = 0;
    }
    if (counter % 50 == 0) {
      struct CircleResult cr;
      if (kasa_method(x_vals_uT, y_vals_uT, KASA_ARRAY_DEPTH, &cr)) {
        printf("KASA Divide by Zero detected\n");
      } else if (cr.rmse > 0.6) {
        calibration_offset = cr;
      }
    }

    vTaskDelay(100);
  }
}