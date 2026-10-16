#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { MEASUREMENT_RING, MEASUREMENT_RECTANGLE } MeasurementGeometry;
typedef enum { MEASUREMENT_VALID, MEASUREMENT_INVALID, MEASUREMENT_PENDING } MeasurementStatus;

/* All dimensions and residuals are in 0.01 mm. No SDL or algorithm dependency.
 * Deliver page events on the LVGL thread. Only completed dimensions are checked. */
typedef struct {
    MeasurementGeometry geometry;
    int32_t primary_cmm;       /* outer diameter / length */
    int32_t secondary_cmm;     /* inner diameter / width */
    uint32_t residual_cmm;
    MeasurementStatus status;
    bool connected;
    bool calibrated;
    bool simulated;           /* transport provenance; never means real hardware */
} MeasurementResult;

bool measurement_result_check(const MeasurementResult * result);
void measurement_format_cmm(char * buffer, size_t capacity, uint32_t value);

#endif
