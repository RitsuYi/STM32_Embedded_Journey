#include "gui_demo.h"
#include <math.h>

/* Explicit simulation backend, owned by the GUI task. Replace these service
 * callbacks with queued UART results when the MaixCAM2 protocol is available. */
static struct {
    MeasurementPage *page;
    InstrumentModel *model;
    lv_timer_t *timer;
    uint32_t request_id, started, samples;
    MeasurementGeometry geometry;
} demo;

static bool start(void *context, uint32_t id, MeasurementGeometry geometry)
{
    (void)context;
    demo.request_id = id; demo.started = lv_tick_get();
    demo.geometry = geometry; demo.samples = demo.model->settings.sample_count;
    lv_timer_reset(demo.timer); lv_timer_resume(demo.timer);
    return true;
}
static void cancel(void *context, uint32_t id)
{
    (void)context;
    if(demo.request_id != id) return;
    demo.request_id = 0; lv_timer_pause(demo.timer);
}
static void step(lv_timer_t *timer)
{
    (void)timer;
    uint32_t now = lv_tick_get();
    measurement_page_process(demo.page, now);
    if(!measurement_session_busy(&demo.page->session)) { lv_timer_pause(demo.timer); return; }
    uint32_t elapsed = now - demo.started;
    if(elapsed < 1800U) {
        measurement_page_progress(demo.page, demo.request_id,
            elapsed < 800U ? MEASUREMENT_ACQUIRING : MEASUREMENT_FITTING,
            (uint8_t)(elapsed * 100U / 1800U));
        return;
    }
        MeasurementResult r = {
        .geometry = demo.geometry,
        .status = MEASUREMENT_VALID,
        .connected = true,
        .calibrated = instrument_calibration_ready(demo.model),
        .simulated = true
    };
    const InstrumentCalibration *c = &demo.model->calibration;

    if (demo.geometry == MEASUREMENT_RECTANGLE) {
        /*
         * 模拟一个旋转摆放的矩形，四角按沿边顺序排列。
         * 这些是图像像素坐标；经过50mm参考件的模拟标定后，
         * 对应的矩形长宽为32mm和20mm。
         */
        static const InstrumentPoint pixels[4] = {
            {200, 100}, {404.8, 253.6},
            {308.8, 381.6}, {104, 228}
        };
        InstrumentPoint mm[4];

        /* 高分辨率配置中，同一位置的像素坐标扩大2倍。 */
        double image_scale = c->image_profile ? 2.0 : 1.0;
        bool ok = r.calibrated;

        for (unsigned i = 0; ok && i < 4U; ++i) {
            InstrumentPoint pixel = {
                pixels[i].x * image_scale,
                pixels[i].y * image_scale
            };

            /* 先通过已有标定参数，把像素坐标换成毫米坐标。 */
            ok = instrument_map_point(c, pixel, &mm[i]);
        }

        /* 再调用你写的函数，得到单位为0.01mm的整数长宽。 */
        if (ok) {
            ok = instrument_rectangle_dimensions(
                mm, &r.primary_cmm, &r.secondary_cmm);
        }

        /*
         * 失败时保留零尺寸，由已有完成接口报告数据异常，
         * 避免把错误结果作为正常测量记录保存。
         */
        if (!ok) {
            r.primary_cmm = 0;
            r.secondary_cmm = 0;
            r.status = MEASUREMENT_INVALID;
        }
    }
    else {
        /* 圆环继续使用现有模拟数据及标定换算流程。 */
        r.primary_cmm = 3012;
        r.secondary_cmm = 1206;
        double scale = c->image_profile ? 16.0 : 8.0;
        InstrumentPoint origin = c->image_profile
            ? (InstrumentPoint){160, 80}
            : (InstrumentPoint){80, 40};
        InstrumentPoint base, end;

        if (r.calibrated) {
            if (instrument_map_point(c, origin, &base) &&
                instrument_map_point(c,
                    (InstrumentPoint){
                        origin.x + r.primary_cmm / 100.0 * scale,
                        origin.y
                    }, &end)) {
                r.primary_cmm = (int32_t)lround(
                    hypot(end.x - base.x, end.y - base.y) * 100);
            }

            if (instrument_map_point(c, origin, &base) &&
                instrument_map_point(c,
                    (InstrumentPoint){
                        origin.x,
                        origin.y + r.secondary_cmm / 100.0 * scale
                    }, &end)) {
                r.secondary_cmm = (int32_t)lround(
                    hypot(end.x - base.x, end.y - base.y) * 100);
            }
        }
    }
    if(r.residual_cmm > demo.model->settings.quality_cmm) r.status = MEASUREMENT_INVALID;
    uint32_t id = demo.request_id;
    demo.request_id = 0; lv_timer_pause(demo.timer);
    measurement_page_complete(demo.page, id, &r);
}
void GUI_DemoInit(MeasurementPage *page, InstrumentModel *model)
{
    demo.page = page; demo.model = model;
    demo.timer = lv_timer_create(step, 100, NULL);
    LV_ASSERT_MALLOC(demo.timer);
    lv_timer_pause(demo.timer);
    const MeasurementBackend backend = {.start = start, .cancel = cancel};
    measurement_page_set_backend(page, &backend);
}
bool GUI_DemoCalibration(void *context, uint32_t profile, InstrumentPoint points[4],
                         InstrumentPoint *verification, uint32_t *expected)
{
    (void)context;
    if(!points || !verification || !expected || profile > 1) return false;
    double scale = profile ? 2.0 : 1.0;
    const InstrumentPoint fixture[] = {{80,40},{480,40},{480,440},{80,440}};
    for(unsigned i = 0; i < 4; ++i) points[i] = (InstrumentPoint){fixture[i].x * scale, fixture[i].y * scale};
    *verification = (InstrumentPoint){240 * scale,40 * scale}; *expected = 2000;
    return true;
}
