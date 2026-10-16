#include "instrument_model.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void instrument_defaults(InstrumentModel * m)
{
    memset(m, 0, sizeof(*m));
    instrument_default_settings(&m->settings);
    m->next_id = 1;
}
void instrument_default_settings(InstrumentSettings *s)
{
    *s = (InstrumentSettings){.mode = MEASUREMENT_RING, .sample_count = 128,
        .quality_cmm = 20, .timeout_ms = 5000, .grid = true, .dimensions = true, .contour = true};
}

bool instrument_settings_check(const InstrumentSettings * s)
{
    return s && (s->mode == MEASUREMENT_RING || s->mode == MEASUREMENT_RECTANGLE) &&
        s->sample_count >= 32 && s->sample_count <= 512 && s->quality_cmm >= 1 && s->quality_cmm <= 100 &&
        s->timeout_ms >= 2000 && s->timeout_ms <= 30000 && s->timeout_ms % 1000 == 0 && s->image_profile <= 1;
}

bool instrument_calibration_ready(const InstrumentModel * m)
{
    return m && m->calibration.valid && m->calibration.image_profile == m->settings.image_profile;
}

bool instrument_apply_settings(InstrumentModel * m, const InstrumentSettings * s)
{
    if(!m || !instrument_settings_check(s)) return false;
    if(m->settings.image_profile != s->image_profile) m->calibration.valid = false;
    m->settings = *s;
    return true;
}

bool instrument_map_point(const InstrumentCalibration * c, InstrumentPoint p, InstrumentPoint * mm)
{
    if(!c || !mm || !isfinite(p.x) || !isfinite(p.y)) return false;
    const double * h = c->homography;
    double w = h[6] * p.x + h[7] * p.y + h[8];
    if(!isfinite(w) || fabs(w) < 1e-10) return false;
    mm->x = (h[0] * p.x + h[1] * p.y + h[2]) / w;
    mm->y = (h[3] * p.x + h[4] * p.y + h[5]) / w;
    return isfinite(mm->x) && isfinite(mm->y);
}

bool instrument_calculate_calibration(const InstrumentPoint p[4], uint32_t reference,
                                      InstrumentPoint verify, uint32_t expected, uint32_t profile,
                                      InstrumentCalibration * out)
{
    if(!p || !out || reference < 100 || reference > 20000 || !expected || profile > 1) return false;
    double a[8][9] = {{0}};
    double side = reference / 100.0;
    double winding = 0;
    for(unsigned i = 0; i < 4; ++i) {
        InstrumentPoint a0 = p[i], b0 = p[(i+1)%4], c0 = p[(i+2)%4];
        double cross = (b0.x-a0.x)*(c0.y-b0.y) - (b0.y-a0.y)*(c0.x-b0.x);
        if(!isfinite(cross) || fabs(cross) < 1e-8 || (winding && cross*winding <= 0)) return false;
        winding = cross;
    }
    const double xy[4][2] = {{0,0}, {1,0}, {1,1}, {0,1}};
    for(unsigned i = 0; i < 4; ++i) {
        if(!isfinite(p[i].x) || !isfinite(p[i].y)) return false;
        double x = p[i].x, y = p[i].y, u = xy[i][0] * side, v = xy[i][1] * side;
        double row0[9] = {x,y,1,0,0,0,-u*x,-u*y,u};
        double row1[9] = {0,0,0,x,y,1,-v*x,-v*y,v};
        memcpy(a[i*2], row0, sizeof(row0)); memcpy(a[i*2+1], row1, sizeof(row1));
    }
    for(unsigned col = 0; col < 8; ++col) {
        unsigned pivot = col;
        for(unsigned row = col + 1; row < 8; ++row)
            if(fabs(a[row][col]) > fabs(a[pivot][col])) pivot = row;
        if(fabs(a[pivot][col]) < 1e-10) return false;
        for(unsigned j = col; j < 9; ++j) {
            double tmp = a[col][j]; a[col][j] = a[pivot][j]; a[pivot][j] = tmp;
        }
        double divisor = a[col][col];
        for(unsigned j = col; j < 9; ++j) a[col][j] /= divisor;
        for(unsigned row = 0; row < 8; ++row) {
            if(row == col) continue;
            double factor = a[row][col];
            for(unsigned j = col; j < 9; ++j) a[row][j] -= factor * a[col][j];
        }
    }
    InstrumentCalibration c = {.simulated = true, .image_profile = profile, .reference_cmm = reference};
    for(unsigned i = 0; i < 8; ++i) { c.homography[i] = a[i][8]; if(!isfinite(a[i][8])) return false; }
    c.homography[8] = 1;
    InstrumentPoint result;
    if(!instrument_map_point(&c, verify, &result)) return false;
    double error = fabs(hypot(result.x, result.y) * 100.0 - expected);
    if(!isfinite(error) || error > UINT32_MAX) return false;
    c.error_cmm = (uint32_t)ceil(fmax(0, error-1e-6));
    c.valid = error <= 50.0 + 1e-6; /* independent 20 mm check, <= 0.50 mm */
    *out = c;
    return true;
}

bool instrument_add_record(InstrumentModel * m, const MeasurementResult * r)
{
    if(!m || !measurement_result_check(r) || r->status == MEASUREMENT_PENDING || !m->next_id || m->next_id == UINT32_MAX) return false;
    uint32_t n = m->record_count < INSTRUMENT_RECORD_CAPACITY ? m->record_count++ : INSTRUMENT_RECORD_CAPACITY - 1;
    memmove(m->records + 1, m->records, n * sizeof(m->records[0]));
    m->records[0] = (InstrumentRecord){.id = m->next_id++, .calibration_version = m->calibration.version, .result = *r};
    return true;
}

bool instrument_delete_record(InstrumentModel * m, uint32_t id)
{
    if(!m) return false;
    for(uint32_t i = 0; i < m->record_count; ++i) {
        if(m->records[i].id != id) continue;
        memmove(m->records + i, m->records + i + 1, (m->record_count - i - 1) * sizeof(m->records[0]));
        --m->record_count;
        memset(m->records + m->record_count, 0, sizeof(m->records[0]));
        return true;
    }
    return false;
}

bool instrument_model_check(const InstrumentModel * m)
{
    if(!m || !instrument_settings_check(&m->settings) || m->record_count > INSTRUMENT_RECORD_CAPACITY || !m->next_id) return false;
    const InstrumentCalibration * c = &m->calibration;
    if(c->image_profile > 1) return false;
    for(unsigned i = 0; i < 9; ++i) if(!isfinite(c->homography[i])) return false;
    if(c->valid && (!c->version || c->reference_cmm < 100 || c->reference_cmm > 20000 || c->error_cmm > 50 || c->homography[8] != 1)) return false;
    if(c->valid) {
        const double * h = c->homography;
        double determinant = h[0]*(h[4]*h[8]-h[5]*h[7]) - h[1]*(h[3]*h[8]-h[5]*h[6]) + h[2]*(h[3]*h[7]-h[4]*h[6]);
        if(!isfinite(determinant) || fabs(determinant) < 1e-12) return false;
    }
    for(uint32_t i = 0; i < m->record_count; ++i) {
        const InstrumentRecord * r = m->records + i;
        if(!r->id || r->id >= m->next_id || (i && r->id >= m->records[i-1].id) ||
           !measurement_result_check(&r->result) || r->result.status == MEASUREMENT_PENDING) return false;
    }
    return true;
}

void instrument_format_dimension(char * b, size_t n, uint32_t cmm, bool inches)
{
    if(inches) snprintf(b, n, "%.3f", cmm / 2540.0);
    else measurement_format_cmm(b, n, cmm);
}
