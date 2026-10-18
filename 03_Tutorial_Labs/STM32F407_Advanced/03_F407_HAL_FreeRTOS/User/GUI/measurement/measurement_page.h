#ifndef MEASUREMENT_PAGE_H
#define MEASUREMENT_PAGE_H

#include "geometry_view.h"
#include "measurement_session.h"

/* A start callback must return immediately. Deliver progress/results later on
 * the LVGL thread. cancel stops timed-out/disconnected requests if supported. */
typedef struct {
    bool (*start)(void * context, uint32_t request_id, MeasurementGeometry geometry);
    void (*cancel)(void * context, uint32_t request_id);
    void * context;
} MeasurementBackend;

typedef struct {
    lv_obj_t * root;
    lv_obj_t * header;
    lv_obj_t * result_panel;
    GeometryView geometry;
    lv_obj_t * navigation[4];
    lv_obj_t * start_button;
    lv_obj_t * start_label;
    lv_obj_t * progress_bar;
    lv_obj_t * caption;
    MeasurementSession session;
    MeasurementSession displayed;
    MeasurementBackend backend;
    unsigned selected_navigation;
    void (*navigation_changed)(void * context, unsigned page);
    void (*result_completed)(void * context, const MeasurementResult * result);
    void * application_context;
    bool inches;
    bool calibration_capture_active;
    bool notice_warning;
    char primary_text[20], secondary_text[20], residual_text[20];
    char hint_text[64], caption_text[96];
    /* 新主界面仍复用原测量会话；质检汇总由唯一GUI任务填入，绘制时只读。 */
    bool quality_mode, automatic, wifi_connected;
    uint32_t total_count, pass_count, fail_count;
    unsigned verdict; /* 与InstrumentVerdict对应，避免显示层依赖业务头文件产生循环包含。 */
    char recent[2][80];
    struct { uint32_t id, uptime_ms; int32_t primary_cmm, secondary_cmm; bool pass; } recent_records[2];
    void (*primary_action)(void *context); /* 中键：单次测量或自动质检开关。 */
    lv_obj_t * zoom[3], * standard_button;
} MeasurementPage;

/* initial_result supplies only geometry/device metadata. Startup is always
 * idle with no measured values or contour, even if given a valid fixture. */
void measurement_page_create(MeasurementPage * page, lv_obj_t * screen,
                             const MeasurementResult * initial_result);
void measurement_page_set_backend(MeasurementPage * page, const MeasurementBackend * backend);
bool measurement_page_start(MeasurementPage * page);
bool measurement_page_select_geometry(MeasurementPage * page, MeasurementGeometry geometry);
bool measurement_page_progress(MeasurementPage * page, uint32_t request_id,
                               MeasurementState phase, uint8_t progress);
bool measurement_page_complete(MeasurementPage * page, uint32_t request_id, const MeasurementResult * result);
bool measurement_page_fail(MeasurementPage * page, uint32_t request_id, MeasurementError error);
void measurement_page_set_device(MeasurementPage * page, bool connected, bool calibrated);
void measurement_page_process(MeasurementPage * page, uint32_t now_ms);
bool measurement_page_set_scale(MeasurementPage * page, uint32_t pixels_per_mm_q8);
void measurement_page_navigate(MeasurementPage * page, unsigned destination);
void measurement_page_set_units(MeasurementPage * page, bool inches);
void measurement_page_set_calibration_capture(MeasurementPage * page, bool active);
void measurement_page_invalidate_result(MeasurementPage * page);
void measurement_page_show_notice(MeasurementPage * page, const char * text, bool warning);

#endif
