#include "lvgl_port.h"
#include "ST7796.h"
#include "gui_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); exit(1); } } while(0)

/* 运行真实LVGL和lvgl_port，只把LCD替换成延迟读取源缓冲的接收端。
 * 到Wait才读取像素，可抓出“Start后提前归还缓冲，被下一轮绘图覆盖”的错误。 */
static uint32_t now_ms, starts, waits, overlap_draws, fail_start, fail_wait;
static bool pending;
static const uint8_t *source, *previous_source;
static uint32_t px, py, pw, ph, pitch;
static uint8_t snapshot[GUI_DRAW_BUFFER_BYTES], framebuffer[480U * 320U * 2U];

uint32_t HAL_GetTick(void) { return now_ms; }
void GUI_LVGL_AssertFailed(const char *file, int line)
{
    fprintf(stderr,"LVGL assert %s:%d\n",file,line); exit(1);
}
HAL_StatusTypeDef ST7796_DMAInit(void) { return HAL_OK; }
HAL_StatusTypeDef ST7796_WriteRGB565Start(uint16_t x,uint16_t y,uint16_t w,uint16_t h,
                                         const uint8_t *pixels,uint32_t stride)
{
    CHECK(!pending && pixels && w && h && x+w<=480 && y+h<=320 && stride>=w*2U);
    ++starts;
    if(starts == fail_start) return HAL_ERROR;
    CHECK(pixels != previous_source); /* 相邻成功块须轮换两块真实绘图缓冲。 */
    previous_source = pixels;
    px=x; py=y; pw=w; ph=h; pitch=stride; source=pixels;
    for(uint32_t row=0;row<ph;++row)
        memcpy(snapshot+row*pw*2U,source+row*pitch,pw*2U);
    pending=true;
    return HAL_OK;
}
HAL_StatusTypeDef ST7796_WriteRGB565Wait(void)
{
    CHECK(pending);
    ++waits;
    for(uint32_t row=0;row<ph;++row) {
        CHECK(memcmp(snapshot+row*pw*2U,source+row*pitch,pw*2U)==0);
        if(waits != fail_wait)
            memcpy(framebuffer+((py+row)*480U+px)*2U,source+row*pitch,pw*2U);
    }
    now_ms+=3U; /* 可控延迟用于检查末块等待也被计入整帧耗时。 */
    pending=false;
    return waits == fail_wait ? HAL_TIMEOUT : HAL_OK;
}
static void observe_draw(lv_event_t *event)
{
    (void)event;
    if(pending) ++overlap_draws;
}
static GUI_DisplayStats stats(void)
{
    GUI_DisplayStats value; GUI_DisplayGetStats(&value); return value;
}
static void redraw(lv_display_t *display)
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
    CHECK(!pending);
}
static void recover(lv_display_t *display)
{
    /* 错误恢复必须在当前刷新返回后执行，不能在flush中递归修改脏区列表。 */
    now_ms+=101U;
    GUI_DisplayService();
    previous_source=NULL; /* 错误块可能未启动，不约束跨失败事务的缓冲相位。 */
    lv_refr_now(display);
    CHECK(!pending);
}
static void run_application(uint32_t milliseconds)
{
    /* 用当前正式应用与5ms步进推进动画，覆盖真实图像、文字及设置页刷新。
     * 不使用旧四导航/现场标定对象，避免把历史界面测试当作当前交互契约。 */
    for(uint32_t elapsed=0;elapsed<milliseconds;elapsed+=5U) {
        now_ms+=5U; GUI_AppProcess(); (void)lv_timer_handler(); GUI_DisplayService();
        CHECK(!pending && lv_mem_test()==LV_RESULT_OK);
    }
}
int main(void)
{
    lv_init(); lv_tick_set_cb(HAL_GetTick);
    lv_display_t *display=GUI_DisplayInit(); CHECK(display);
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_hex(0xFF0000),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_add_event_cb(screen,observe_draw,LV_EVENT_DRAW_MAIN,NULL);
    redraw(display);
    GUI_DisplayStats first=stats();
    CHECK(starts==10 && waits==10 && overlap_draws>0);
    CHECK(first.completed_frames==1 && first.last_frame_blocks==10);
    CHECK(first.last_frame_bytes==307200U && first.last_frame_ms==30U && first.last_frame_wait_ms==30U);
    for(uint32_t i=0;i<480U*320U;++i) CHECK(framebuffer[2*i]==0 && framebuffer[2*i+1]==0xF8);
    lv_refr_now(display); CHECK(starts==10 && stats().completed_frames==1);

    /* 窄局部矩形由真实LVGL生成，验证只发送脏区，不强制每帧刷新整屏。 */
    lv_area_t area={13,17,15,18};
    lv_obj_invalidate_area(screen,&area); lv_refr_now(display);
    CHECK(stats().last_frame_bytes==12U && stats().last_frame_blocks==1U && !pending);

    uint32_t completed=stats().completed_frames;
    fail_start=starts+1U;
    previous_source=NULL;
    redraw(display);
    CHECK(stats().completed_frames==completed && stats().transfer_errors==1U);
    fail_start=0; recover(display);
    CHECK(stats().completed_frames==completed+1U);

    /* 末块等待失败也不能把该帧计为成功；下一次限速完整重绘可恢复。 */
    completed=stats().completed_frames;
    fail_wait=waits+10U;
    redraw(display);
    CHECK(stats().completed_frames==completed && stats().transfer_errors==2U);
    fail_wait=0; recover(display);
    CHECK(stats().completed_frames==completed+1U);
    for(uint32_t i=0;i<480U*320U;++i) CHECK(framebuffer[2*i]==0 && framebuffer[2*i+1]==0xF8);
    printf("PASS: LVGL double buffer, %lu draws while DMA pending, source lifetime, final wait, partial pixels, failure recovery\n",
           (unsigned long)overlap_draws);
    /* 专用像素检查之后重新创建显示，独立验证当前主页/设置与原模拟业务。 */
    lv_deinit(); lv_init(); lv_tick_set_cb(HAL_GetTick);
    previous_source=NULL; display=GUI_DisplayInit(); CHECK(display);
    GUI_AppInit(NULL); run_application(100U);
    for(unsigned cycle=0;cycle<4;++cycle) {
        GUI_AppKey(2); run_application(250U);
        GUI_AppKey(2); run_application(200U);
    }
    GUI_AppKey(3); run_application(2200U);
    CHECK(stats().transfer_errors==0 && stats().completed_frames>10);
    puts("PASS: current dashboard/settings/measurement through real DMA display adapter");
    lv_deinit(); return 0;
}
