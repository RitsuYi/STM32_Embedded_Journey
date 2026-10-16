#include "measurement_session.h"

void measurement_session_init(MeasurementSession * s, MeasurementGeometry geometry,
                              bool connected, bool calibrated, bool simulated)
{
    *s = (MeasurementSession){
        .geometry = geometry, .state = MEASUREMENT_IDLE,
        .connected = connected, .calibrated = calibrated, .simulated = simulated,
        .timeout_ms = MEASUREMENT_TIMEOUT_MS,
        .result = {.geometry = geometry, .status = MEASUREMENT_PENDING}
    };
}

bool measurement_session_busy(const MeasurementSession * s)
{
    return s && (s->state == MEASUREMENT_ACQUIRING || s->state == MEASUREMENT_FITTING);
}

bool measurement_session_select(MeasurementSession * s, MeasurementGeometry geometry)
{
    if(!s || measurement_session_busy(s) ||
       (geometry != MEASUREMENT_RING && geometry != MEASUREMENT_RECTANGLE)) return false;
    if(s->geometry == geometry) return true;
    s->geometry = geometry;
    s->state = MEASUREMENT_IDLE;
    s->error = MEASUREMENT_ERROR_NONE;
    s->progress = 0;
    s->has_result = false;
    s->result = (MeasurementResult){.geometry = geometry, .status = MEASUREMENT_PENDING};
    return true;
}

bool measurement_session_start(MeasurementSession * s, uint32_t now_ms)
{
    if(!s || measurement_session_busy(s)) return false;
    s->progress = 0;
    s->error = !s->connected ? MEASUREMENT_ERROR_DISCONNECTED :
               (!s->calibrated ? MEASUREMENT_ERROR_UNCALIBRATED : MEASUREMENT_ERROR_NONE);
    if(s->error != MEASUREMENT_ERROR_NONE) {
        s->state = MEASUREMENT_FAILED;
        return false;
    }
    if(++s->request_id == 0) ++s->request_id; /* zero is never an active request */
    s->started_ms = now_ms;
    s->state = MEASUREMENT_ACQUIRING;
    return true;
}

bool measurement_session_progress(MeasurementSession * s, uint32_t id,
                                  MeasurementState phase, uint8_t progress)
{
    if(!measurement_session_busy(s) || id != s->request_id || progress > 99 ||
       progress < s->progress || (phase != MEASUREMENT_ACQUIRING && phase != MEASUREMENT_FITTING) ||
       (s->state == MEASUREMENT_FITTING && phase != MEASUREMENT_FITTING)) return false;
    s->state = phase;
    s->progress = progress;
    return true;
}

bool measurement_session_fail(MeasurementSession * s, uint32_t id, MeasurementError error)
{
    if(!measurement_session_busy(s) || id != s->request_id ||
       error <= MEASUREMENT_ERROR_NONE || error > MEASUREMENT_ERROR_RESULT) return false;
    s->state = MEASUREMENT_FAILED;
    s->error = error;
    s->progress = 0;
    return true;
}

bool measurement_session_complete(MeasurementSession * s, uint32_t id, const MeasurementResult * r)
{
    if(!measurement_session_busy(s) || id != s->request_id) return false;
    if(!measurement_result_check(r) || r->status == MEASUREMENT_PENDING || r->geometry != s->geometry) {
        measurement_session_fail(s, id, MEASUREMENT_ERROR_RESULT);
        return false;
    }
    if(!r->connected || !r->calibrated) {
        measurement_session_set_device(s, r->connected, r->calibrated);
        return false;
    }
    s->result = *r;
    s->result.simulated = s->simulated;
    s->has_result = true;
    s->state = MEASUREMENT_COMPLETE;
    s->error = MEASUREMENT_ERROR_NONE;
    s->progress = 100;
    return true;
}

void measurement_session_set_device(MeasurementSession * s, bool connected, bool calibrated)
{
    if(!s) return;
    s->connected = connected;
    s->calibrated = calibrated;
    if(measurement_session_busy(s) && (!connected || !calibrated))
        measurement_session_fail(s, s->request_id, !connected ? MEASUREMENT_ERROR_DISCONNECTED :
                                                               MEASUREMENT_ERROR_UNCALIBRATED);
}

bool measurement_session_process(MeasurementSession * s, uint32_t now_ms)
{
    /* Unsigned subtraction also works across the millisecond counter wrap. */
    return measurement_session_busy(s) && (uint32_t)(now_ms - s->started_ms) >= s->timeout_ms &&
           measurement_session_fail(s, s->request_id, MEASUREMENT_ERROR_TIMEOUT);
}

