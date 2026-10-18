#ifndef INSTRUMENT_UI_H
#define INSTRUMENT_UI_H
#include "instrument_model.h"
#include "../measurement/measurement_page.h"

typedef struct {
    bool (*save)(void * context, const InstrumentModel * model);
    bool (*load_calibration)(void * context, InstrumentCalibration * calibration);
    bool (*capture_calibration)(void * context, uint32_t profile, InstrumentPoint points[4],
                                InstrumentPoint * verification, uint32_t * expected_cmm);
    void * context;
    void * calibration_context;
    bool persistent;
    const char * storage_name;
} InstrumentServices;
typedef enum { EDIT_NONE, EDIT_REFERENCE, EDIT_SAMPLES, EDIT_QUALITY, EDIT_TIMEOUT } InstrumentEdit;
typedef enum { CONFIRM_NONE, CONFIRM_DELETE, CONFIRM_CLEAR, CONFIRM_DEFAULTS, CONFIRM_PROFILE } InstrumentConfirm;
typedef enum { SETTINGS_MENU, SETTINGS_MEASUREMENT, SETTINGS_DISPLAY, SETTINGS_DEVICE } InstrumentSettingsPage;
typedef struct {
    MeasurementPage * measurement;
    InstrumentModel * model;
    InstrumentServices services;
    InstrumentModel rollback; /* GUI-owned transaction snapshot, not on task stack. */
    lv_obj_t * panel, * title, * subtitle, * footer, * rows[5], * row_labels[5], * row_values[5];
    lv_obj_t * actions[2], * action_labels[2], * up, * down, * page_label, * preview, * back;
    lv_obj_t * editor, * editor_title, * editor_hint, * textarea, * keyboard;
    lv_obj_t * confirmation, * confirmation_text, * confirm_button;
    lv_timer_t * calibration_timer;
    InstrumentSettingsPage settings_page;
    unsigned records_page, calibration_step;
    uint32_t selected_record, reference_cmm, capture_started;
    uint32_t keyboard_pressed_key; /* 保存本次按住的键号；滑到另一列后取消，避免拖动串键。 */
    uint32_t applied_calibration_version;
    InstrumentCalibration candidate;
    InstrumentEdit edit;
    InstrumentConfirm confirm;
    bool wizard, capturing, storage_failed;
    char message[128];
} InstrumentUI;
void instrument_ui_create(InstrumentUI * ui, MeasurementPage * measurement, InstrumentModel * model,
                          const InstrumentServices * services, const char * startup_message);
void instrument_ui_refresh(InstrumentUI * ui);
void instrument_ui_apply(InstrumentUI * ui);
bool instrument_ui_set_mode(InstrumentUI * ui, MeasurementGeometry mode);
bool instrument_ui_input_active(const InstrumentUI * ui);
void instrument_ui_destroy(InstrumentUI * ui);
#endif
