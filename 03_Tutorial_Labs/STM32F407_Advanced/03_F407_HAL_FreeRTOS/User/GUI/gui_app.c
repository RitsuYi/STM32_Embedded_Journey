#include "gui_app.h"
#include "gui_demo.h"
#include <string.h>

static MeasurementPage page;
static InstrumentModel model;
static InstrumentUI ui;
static GUI_StorageOps storage;

static bool save(void *context, const InstrumentModel *value)
{
    (void)context;
    return storage.save && storage.save(storage.context, value);
}
static bool load_calibration(void *context, InstrumentCalibration *calibration)
{
    (void)context;
    if(!storage.load || !storage.load(storage.context, &ui.rollback) ||
       !instrument_model_check(&ui.rollback)) return false;
    *calibration = ui.rollback.calibration;
    return true;
}
void GUI_AppInit(const GUI_StorageOps *ops)
{
    storage = ops ? *ops : (GUI_StorageOps){0};
    instrument_defaults(&model);
    bool loaded = storage.load && storage.load(storage.context, &model) && instrument_model_check(&model);
    if(!loaded) instrument_defaults(&model);
    MeasurementResult initial = {.geometry = model.settings.mode, .status = MEASUREMENT_PENDING,
        .connected = true, .calibrated = false, .simulated = true};
    measurement_page_create(&page, lv_screen_active(), &initial);
    GUI_DemoInit(&page, &model);
    InstrumentServices services = {.save = storage.save ? save : NULL,
        .load_calibration = load_calibration, .capture_calibration = GUI_DemoCalibration,
        .persistent = storage.save != NULL, .storage_name = storage.name,
        .context = NULL, .calibration_context = NULL};
    instrument_ui_create(&ui, &page, &model, &services,
        loaded ? "已恢复设置、标定与测量记录" : "模拟测量 / 仅内存 / 请先完成标定");
}
static void click(lv_obj_t *object)
{
    if(object && !lv_obj_is_hidden(object) &&
       !lv_obj_has_state(object, LV_STATE_DISABLED)) lv_obj_send_event(object, LV_EVENT_CLICKED, NULL);
}
void GUI_AppKey(unsigned key)
{
    if(instrument_ui_input_active(&ui)) return; /* numeric editing requires touch */
    if(key == 1 || key == 2) {
        unsigned next = (page.selected_navigation + (key == 1 ? 3U : 1U)) % 4U;
        measurement_page_navigate(&page, next);
    }
    else if(key == 3) {
        switch(page.selected_navigation) {
            case 0: measurement_page_start(&page); break;
            case 1: click(ui.actions[0]); break;
            case 2:
                if(ui.selected_record || !model.record_count) click(ui.actions[0]);
                else click(ui.rows[0]);
                break;
            case 3: click(ui.rows[0]); break;
        }
    }
}
void GUI_AppProcess(void)
{
    measurement_page_process(&page, lv_tick_get());
}
