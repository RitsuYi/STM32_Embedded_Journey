#ifndef MEASUREMENT_SESSION_H
#define MEASUREMENT_SESSION_H

#include "measurement.h"

#define MEASUREMENT_TIMEOUT_MS 5000U

typedef enum {
    MEASUREMENT_IDLE,
    MEASUREMENT_ACQUIRING,
    MEASUREMENT_FITTING,
    MEASUREMENT_COMPLETE,
    MEASUREMENT_FAILED
} MeasurementState;

typedef enum {
    MEASUREMENT_ERROR_NONE,
    MEASUREMENT_ERROR_DISCONNECTED,
    MEASUREMENT_ERROR_UNCALIBRATED,
    MEASUREMENT_ERROR_BACKEND,
    MEASUREMENT_ERROR_TIMEOUT,
    MEASUREMENT_ERROR_RESULT
} MeasurementError;

/* Portable event state machine. No LVGL, SDL, allocation, polling loop or clock
 * dependency. The owner supplies monotonic milliseconds and serializes events. */
typedef struct {
    MeasurementGeometry geometry;
    MeasurementState state;
    MeasurementError error;
    MeasurementResult result; /* last completed result, explicitly retained */
    bool has_result;
    bool connected, calibrated, simulated;
    uint8_t progress;
    uint32_t request_id, started_ms, timeout_ms;
} MeasurementSession;

void measurement_session_init(MeasurementSession * session, MeasurementGeometry geometry,
                              bool connected, bool calibrated, bool simulated);
bool measurement_session_busy(const MeasurementSession * session);
bool measurement_session_select(MeasurementSession * session, MeasurementGeometry geometry);
bool measurement_session_start(MeasurementSession * session, uint32_t now_ms);
bool measurement_session_progress(MeasurementSession * session, uint32_t request_id,
                                  MeasurementState phase, uint8_t progress);
bool measurement_session_complete(MeasurementSession * session, uint32_t request_id,
                                  const MeasurementResult * result);
bool measurement_session_fail(MeasurementSession * session, uint32_t request_id, MeasurementError error);
void measurement_session_set_device(MeasurementSession * session, bool connected, bool calibrated);
bool measurement_session_process(MeasurementSession * session, uint32_t now_ms);

#endif
