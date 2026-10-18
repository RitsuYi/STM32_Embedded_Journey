#ifndef INSTRUMENT_MODEL_H
#define INSTRUMENT_MODEL_H
#include "../measurement/measurement.h"

#define INSTRUMENT_RECORD_CAPACITY 64U

typedef struct {
    MeasurementGeometry mode;
    uint32_t sample_count, quality_cmm, timeout_ms;
    bool inches, grid, dimensions, contour;
    uint32_t image_profile; /* 0: 640x480; 1: 1280x960 */
} InstrumentSettings;
typedef struct {
    bool valid, simulated;
    uint32_t version, image_profile, reference_cmm, error_cmm;
    double homography[9]; /* pixel -> millimetre plane */
} InstrumentCalibration;
typedef struct {
    uint32_t id, calibration_version;
    MeasurementResult result;
} InstrumentRecord;
typedef struct {
    InstrumentSettings settings;
    InstrumentCalibration calibration;
    InstrumentRecord records[INSTRUMENT_RECORD_CAPACITY]; /* newest first */
    uint32_t record_count, next_id;
} InstrumentModel;
typedef struct { double x, y; } InstrumentPoint;

void instrument_defaults(InstrumentModel * model);
void instrument_default_settings(InstrumentSettings *settings);
bool instrument_settings_check(const InstrumentSettings * settings);
bool instrument_model_check(const InstrumentModel * model);
bool instrument_apply_settings(InstrumentModel * model, const InstrumentSettings * settings);
bool instrument_calibration_ready(const InstrumentModel * model);
bool instrument_calculate_calibration(const InstrumentPoint pixels[4], uint32_t reference_cmm,
                                      InstrumentPoint verification_pixel, uint32_t verification_cmm,
                                      uint32_t profile, InstrumentCalibration * candidate);
bool instrument_map_point(const InstrumentCalibration * calibration, InstrumentPoint pixel, InstrumentPoint * mm);
bool instrument_add_record(InstrumentModel * model, const MeasurementResult * result);
bool instrument_delete_record(InstrumentModel * model, uint32_t id);
void instrument_format_dimension(char * buffer, size_t size, uint32_t cmm, bool inches);

/*
 * 根据已知矩形的四个角点计算长宽。
 * points_mm：按沿边顺序排列的四点，坐标单位为毫米。
 * length_cmm、width_cmm：用于接收结果，1代表0.01毫米。
 * 返回true表示成功；返回false时不修改输出。
 * 调用者须保证输入确实是矩形，本函数只负责尺寸计算。
 */
bool instrument_rectangle_dimensions(const InstrumentPoint points_mm[4],
                                     int32_t *length_cmm,
                                     int32_t *width_cmm);
                                     
#endif
