#include "instrument/dashboard_ui.h"
#include "gui_camera_config.h"
#include "gui_demo.h"
#include "lvgl_port.h"
#include "ST7796.h"
#include "fonts/measurement_fonts.h"
#include "measurement/ui_draw.h"
#include "measurement/ui_icons.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}} while(0)
/* 使用真实LVGL、页面、模拟后端和触摸适配；只替代LCD与实际手指。
 * 307200字节截图缓冲仅在电脑测试程序存在，绝不编入F407固件。 */
static uint16_t frame[480*320];
static uint32_t now;
static int32_t tx,ty;
static bool down;
static MeasurementPage page;
static InstrumentModel model;
static DashboardUI ui;
uint32_t HAL_GetTick(void) {return now;}
void GUI_LVGL_AssertFailed(const char *file,int line)
{lv_mem_monitor_t m;lv_mem_monitor(&m);fprintf(stderr,"LVGL assert %s:%d menu=%u edit=%u time=%lu free=%lu biggest=%lu peak=%lu\n",file,line,ui.menu,ui.edit,(unsigned long)now,(unsigned long)m.free_size,(unsigned long)m.free_biggest_size,(unsigned long)m.max_used);exit(1);}
HAL_StatusTypeDef ST7796_WriteRGB565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint8_t *pixels,uint32_t stride)
{
    CHECK(x+w<=480&&y+h<=320&&stride>=w*2U);
    for(unsigned row=0;row<h;++row)memcpy(frame+(y+row)*480+x,pixels+row*stride,w*2U);
    return HAL_OK;
}
static bool reader(void *context,int32_t *x,int32_t *y,bool *pressed)
{(void)context;*x=tx;*y=ty;*pressed=down;return true;}
static void advance(unsigned ms)
{for(unsigned i=0;i<ms;i+=5){now+=5;measurement_page_process(&page,now);lv_timer_handler();}}
static void tap_at(lv_obj_t *o,int x,int y)
{
    lv_obj_update_layout(lv_screen_active());lv_point_t p={x,y};
    if(lv_indev_search_obj(lv_screen_active(),&p)!=o)fprintf(stderr,"tap mismatch %d,%d expected=%p actual=%p edit=%u menu=%u\n",x,y,(void *)o,(void *)lv_indev_search_obj(lv_screen_active(),&p),ui.edit,ui.menu);
    CHECK(lv_indev_search_obj(lv_screen_active(),&p)==o);
    tx=x;ty=y;down=true;advance(60);down=false;advance(230);
}
static void tap(lv_obj_t *o)
{
    CHECK(o);lv_obj_update_layout(lv_screen_active());lv_area_t a;lv_obj_get_coords(o,&a);
    tap_at(o,(a.x1+a.x2)/2,(a.y1+a.y2)/2);
}
static void screenshot(const char *path)
{
    lv_obj_invalidate(page.root);advance(45);FILE *f=fopen(path,"wb");CHECK(f);
    fprintf(f,"P6\n480 320\n255\n");
    for(unsigned i=0;i<480U*320U;++i){uint16_t p=frame[i];
        fputc(((p>>11)&31)*255/31,f);fputc(((p>>5)&63)*255/63,f);fputc((p&31)*255/31,f);}
    fclose(f);
}
/* 通过真实LVGL绘图和当前flush捕获纯色，检查白色及RGB565分量。
 * 这些是电脑上的数字像素检查，不代表液晶面板亮度/伽马已经实测正确。 */
static void color_contract(void)
{
    const uint32_t colors[]={0xFFFFFF,0xFF0000,0x00FF00,0x0000FF,0x808080,0x000000};
    const uint16_t expected[]={0xFFFF,0xF800,0x07E0,0x001F,0x8410,0x0000};
    lv_obj_t *group=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(group);
    lv_obj_set_size(group,240,24);lv_obj_set_scrollable(group,false);
    for(unsigned i=0;i<6;++i) {
        lv_obj_t *o=lv_obj_create(group);lv_obj_remove_style_all(o);
        lv_obj_set_pos(o,(int)i*40,0);lv_obj_set_size(o,40,24);
        lv_obj_set_style_bg_color(o,ui_color(colors[i]),0);lv_obj_set_style_bg_opa(o,255,0);
    }
    lv_refr_now(lv_display_get_default());
    for(unsigned i=0;i<6;++i)CHECK(frame[12*480+i*40+20]==expected[i]);
    lv_obj_delete(group);
}
/* 固定同一时刻，分别用1/7/20行缓冲重绘，再裁成不对齐的小矩形重绘。
 * 整幅RGB565必须逐字节一致，才能排除步幅、解码起点或局部刷新引入的方块/接缝。 */
static void redraw_contract(void)
{
    lv_display_t *display=lv_display_get_default();
    uint8_t *pixels=lv_display_get_buf_active(display)->data;
    lv_obj_invalidate(page.root);lv_refr_now(display);
    uint16_t *reference=malloc(sizeof(frame));CHECK(reference);memcpy(reference,frame,sizeof(frame));
    const unsigned heights[]={1,7,20};
    for(unsigned i=0;i<3;++i) {
        lv_display_set_buffers(display,pixels,NULL,480U*2U*heights[i],LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_obj_invalidate(page.root);lv_refr_now(display);
        if(memcmp(reference,frame,sizeof(frame))) {
            unsigned count=0;int x1=480,y1=320,x2=0,y2=0;
            for(unsigned p=0;p<480U*320U;++p)if(reference[p]!=frame[p]) {
                int x=(int)(p%480),y=(int)(p/480);x1=LV_MIN(x1,x);y1=LV_MIN(y1,y);x2=LV_MAX(x2,x);y2=LV_MAX(y2,y);
                if(count<8)fprintf(stderr,"pixel %d,%d expected=%04x actual=%04x\n",x,y,reference[p],frame[p]);
                ++count;
            }
            fprintf(stderr,"RGB565 mismatch at strip height %u: %u pixels, area %d,%d..%d,%d\n",heights[i],count,x1,y1,x2,y2);
        }
        CHECK(!memcmp(reference,frame,sizeof(frame)));
    }
    const lv_area_t areas[]={{17,39,87,79},{83,81,139,205},{143,88,261,247},{313,73,466,158}};
    for(unsigned i=0;i<sizeof(areas)/sizeof(areas[0]);++i) {
        lv_obj_invalidate_area(page.root,&areas[i]);lv_refr_now(display);
        CHECK(!memcmp(reference,frame,sizeof(frame)));
    }
    free(reference);puts("PASS: RGB565 white/primary colors, 1/7/20-row and cropped redraw consistency");
}
static unsigned connects,disconnects;
static bool connect_request(void *context,const char *ssid,const char *password)
{(void)context;CHECK(!strcmp(ssid,"Lab_WiFi"));CHECK(!strcmp(password,"demo12345"));++connects;return true;}
static bool disconnect_request(void *context){(void)context;++disconnects;return true;}
static bool reject_save(void *context,const InstrumentModel *m){(void)context;(void)m;return false;}
static bool silent_backend(void *context,uint32_t id,MeasurementGeometry geometry)
{(void)context;(void)id;(void)geometry;return true; /* 模拟已受理但不回结果的相机。 */}
static void font_check(void)
{
    const char *strings[]={"质检合格标准设定当前检测结果累计统计最近检测记录智能视觉测量仪测量设置","网络密码设备信息目标上下偏差断开连接"};
    for(unsigned f=0;f<2;++f)for(unsigned i=0;i<2;++i){
        const lv_font_t *font=f==0?&ui_font_12:&ui_font_14_bold;
        const char *p=strings[i];while(*p){uint32_t cp;unsigned char c=(unsigned char)*p++;
            if(c<128)cp=c;else{cp=c&15;cp=(cp<<6)|((unsigned char)*p++&63);cp=(cp<<6)|((unsigned char)*p++&63);}
            lv_font_glyph_dsc_t d;if(!lv_font_get_glyph_dsc(font,&d,cp,0)) {fprintf(stderr,"Missing glyph U+%04lx font %u\n",(unsigned long)cp,f);exit(1);}}
    }
    /* 图标必须来自独立字体的真实字形，不能由缺字占位框或中文fallback冒充。 */
    const uint32_t icons[]={0xE001,0xF013,0xF545,0xF3ED,0xF46C,0xF080,0xF15C,0xF1EB,0xF00E,
        0xF010,0xF065,0xF058,0xF057,0xF140,0xF04B,0xF04D,0xF054,0xF030};
    const lv_font_t *fonts[]={&ui_icons_16,&ui_icons_20};
    for(unsigned f=0;f<2;++f)for(unsigned i=0;i<sizeof(icons)/sizeof(icons[0]);++i) {
        lv_font_glyph_dsc_t d;CHECK(lv_font_get_glyph_dsc(fonts[f],&d,icons[i],0));
        CHECK(!d.is_placeholder&&d.resolved_font==fonts[f]&&d.box_w>0&&d.box_h>0);
        CHECK(d.ofs_x+(int)d.box_w<=(f?28:24)&&d.box_h<=fonts[f]->line_height);
    }
    lv_point_t size;
    lv_text_get_size(&size,"30.12",&ui_font_26_bold,0,0,LV_COORD_MAX,0);CHECK(size.x<=74);
    lv_text_get_size(&size,"30.12",&ui_font_16_bold,0,0,LV_COORD_MAX,0);CHECK(size.x<=49);
}
/* 同一时刻观察真实渲染后的Wi-Fi区域：连接/未连接必须不同，分隔线位置固定。
 * 只改变软件后端状态，不连接真实网络；两个状态都必须保留蓝色像素。 */
static void wifi_visual_contract(void)
{
    uint16_t disconnected[24*24];lv_display_t *display=lv_display_get_default();
    lv_obj_invalidate(page.header);lv_refr_now(display);
    for(unsigned y=0;y<24;++y)memcpy(disconnected+y*24,frame+(y+3)*480+384,24*2);
    CHECK(frame[15*480+413]==0xD73E);
    dashboard_network_status(&ui,true,"Test","192.0.2.1");lv_refr_now(display);
    unsigned different=0,blue_on=0,blue_off=0;
    for(unsigned y=0;y<24;++y)for(unsigned x=0;x<24;++x) {
        uint16_t on=frame[(y+3)*480+x+384],off=disconnected[y*24+x];different+=(on!=off);
        blue_on+=((on&31)>((on>>11)&31)+5U);blue_off+=((off&31)>((off>>11)&31)+5U);
    }
    CHECK(different>5&&blue_on>10&&blue_off>10&&frame[15*480+413]==0xD73E);
    screenshot("wifi_connected.ppm");dashboard_network_status(&ui,false,NULL,NULL);
    screenshot("wifi_disconnected.ppm");
}
int main(void)
{
    lv_init();lv_tick_set_cb(HAL_GetTick);CHECK(GUI_DisplayInit());CHECK(GUI_RegisterTouch(reader,NULL));font_check();color_contract();
    instrument_defaults(&model);GUI_ApplyFixedCamera(&model);
    MeasurementResult r={.geometry=MEASUREMENT_RING,.status=MEASUREMENT_PENDING,.connected=true,.calibrated=true,.simulated=true};
    measurement_page_create(&page,lv_screen_active(),&r);GUI_DemoInit(&page,&model);dashboard_create(&ui,&page,&model,NULL);advance(60);
    wifi_visual_contract();
    CHECK(page.selected_navigation==0&&!page.quality_mode&&instrument_model_check(&model));
    screenshot("measurement_idle.ppm");
    /* 三个底部键可见区域以下一直到Y319，都必须命中原键，且相邻键不重叠。 */
    lv_obj_t *buttons[]={page.navigation[0],page.start_button,page.navigation[3]};
    int centers[]={81,240,399};
    for(unsigned i=0;i<3;++i)for(int y=310;y<=319;++y){lv_point_t p={centers[i],y};CHECK(lv_indev_search_obj(page.root,&p)==buttons[i]);}
    tap_at(page.start_button,240,319);advance(2000);
    CHECK(page.session.state==MEASUREMENT_COMPLETE&&model.record_count==1);
    CHECK(model.records[0].result.primary_cmm==3012&&!model.records[0].inspection);
    screenshot("measurement.ppm");
    redraw_contract();
    uint32_t scale=page.geometry.scale_q8;tap(page.zoom[0]);CHECK(page.geometry.scale_q8>scale);
    tap(page.zoom[1]);CHECK(page.geometry.scale_q8<4096);tap(page.zoom[2]);CHECK(page.geometry.scale_q8==GEOMETRY_DEFAULT_SCALE_Q8);
    tap_at(page.navigation[0],81,319);CHECK(page.quality_mode&&page.selected_navigation==1);
    screenshot("inspection_idle.ppm"); /* 待检测时也必须保留四个完整标题图标。 */
    tap_at(page.start_button,240,319);CHECK(page.automatic);advance(4500);
    CHECK(model.inspected>=2&&model.passed==model.inspected&&page.total_count==model.inspected);
    screenshot("inspection_pass.ppm");
    tap_at(page.start_button,240,319);CHECK(!page.automatic);
    /* 下一次改变标准后的失败要记到历史；旧记录仍然合格。 */
    model.settings.tolerance[0][0].target_cmm=2900;
    CHECK(measurement_page_start(&page));advance(2000);
    CHECK(model.records[0].verdict==INSPECTION_PRIMARY&&model.failed==1);
    CHECK(model.records[1].verdict==INSPECTION_PASS);
    screenshot("inspection.ppm");
    tap(page.navigation[2]);CHECK(ui.menu==DASH_RECORDS&&!lv_obj_is_hidden(ui.panel));screenshot("records.ppm");
    tap(ui.back);CHECK(lv_obj_is_hidden(ui.panel)&&page.quality_mode);
    tap(page.standard_button);CHECK(ui.menu==DASH_STANDARDS);
    tap(ui.rows[0]);CHECK(ui.edit);
    screenshot("numeric_keyboard.ppm");
    /* 对比底行五键主体与Y319输入结果/光标位置，验证移植后仍保留原补偿。 */
    lv_obj_update_layout(lv_screen_active());
    const lv_buttonmatrix_t *matrix=(const lv_buttonmatrix_t *)ui.keyboard;
    lv_area_t keys;lv_obj_get_coords(ui.keyboard,&keys);
    for(unsigned i=matrix->btn_cnt-5;i<matrix->btn_cnt;++i) {
        int x=keys.x1+(matrix->button_areas[i].x1+matrix->button_areas[i].x2)/2;
        int y=keys.y1+(matrix->button_areas[i].y1+matrix->button_areas[i].y2)/2;
        lv_textarea_set_text(ui.textarea,"12");lv_textarea_set_cursor_pos(ui.textarea,1);
        tap_at(ui.keyboard,x,y);
        char expected[32];snprintf(expected,sizeof(expected),"%s",lv_textarea_get_text(ui.textarea));
        int expected_cursor=lv_textarea_get_cursor_pos(ui.textarea);
        lv_textarea_set_text(ui.textarea,"12");lv_textarea_set_cursor_pos(ui.textarea,1);
        tap_at(ui.keyboard,x,319);
        CHECK(!strcmp(expected,lv_textarea_get_text(ui.textarea))&&expected_cursor==(int)lv_textarea_get_cursor_pos(ui.textarea));
    }
    lv_textarea_set_text(ui.textarea,"30.00");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);advance(50);
    CHECK(!ui.edit&&model.settings.tolerance[0][0].target_cmm==3000);
    /* 保存失败回滚整个设置，编辑器保留，不能显示伪成功。 */
    ui.services.save=reject_save;tap(ui.rows[0]);lv_textarea_set_text(ui.textarea,"31.00");
    lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);CHECK(ui.edit&&model.settings.tolerance[0][0].target_cmm==3000);
    lv_obj_send_event(ui.keyboard,LV_EVENT_CANCEL,NULL);ui.services.save=NULL;
    screenshot("standards.ppm");tap(ui.back);
    tap_at(page.navigation[3],399,319);CHECK(ui.menu==DASH_SETTINGS);screenshot("settings.ppm");
    /* 实际手指上滑：由输入系统驱动容器滚动，不只调用scroll_to伪装成触摸验证。 */
    tx=250;ty=245;down=true;advance(40);
    for(int y=245;y>=105;y-=10){ty=y;advance(20);}down=false;advance(200);
    CHECK(lv_obj_get_scroll_y(ui.body)>0);
    dashboard_open(&ui,DASH_NETWORK);advance(250);tap(ui.rows[0]);CHECK(ui.edit);
    lv_textarea_set_text(ui.textarea,"Lab_WiFi");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    screenshot("wifi_keyboard.ppm");
    lv_textarea_set_text(ui.textarea,"demo12345");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    CHECK(!ui.connected&&!ui.connecting&&strstr(ui.message,"未接入"));
    DashboardNetworkOps net={.connect=connect_request,.disconnect=disconnect_request};dashboard_set_network(&ui,&net);
    tap(ui.rows[0]);lv_textarea_set_text(ui.textarea,"Lab_WiFi");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    lv_textarea_set_text(ui.textarea,"demo12345");lv_obj_send_event(ui.keyboard,LV_EVENT_READY,NULL);
    CHECK(connects==1&&ui.connecting&&!ui.connected&&!ui.password[0]);
    dashboard_network_status(&ui,true,"Lab_WiFi","192.168.1.6");CHECK(ui.connected&&page.wifi_connected);
    screenshot("wifi.ppm");tap(ui.rows[1]);CHECK(disconnects==1&&ui.connected);
    dashboard_network_status(&ui,false,NULL,NULL);CHECK(!ui.connected);
    dashboard_open(&ui,DASH_DEVICE);advance(250);screenshot("device.ppm");
    /* 公差边界两端都合格，超出一单位失败；无效测量不能合格。 */
    InstrumentSettings s=model.settings;MeasurementResult q={.geometry=MEASUREMENT_RING,.primary_cmm=2980,.secondary_cmm=1190,.status=MEASUREMENT_VALID};
    CHECK(instrument_inspect(&s,&q)==INSPECTION_PASS);q.primary_cmm=3020;q.secondary_cmm=1210;CHECK(instrument_inspect(&s,&q)==INSPECTION_PASS);
    ++q.primary_cmm;CHECK(instrument_inspect(&s,&q)==INSPECTION_PRIMARY);q.primary_cmm=3000;++q.secondary_cmm;CHECK(instrument_inspect(&s,&q)==INSPECTION_SECONDARY);
    q.status=MEASUREMENT_INVALID;CHECK(instrument_inspect(&s,&q)==INSPECTION_INVALID);
    dashboard_close(&ui);advance(250);measurement_page_navigate(&page,0);
    model.settings.mode=MEASUREMENT_RECTANGLE;measurement_page_select_geometry(&page,MEASUREMENT_RECTANGLE);
    CHECK(measurement_page_start(&page));advance(2000);CHECK(page.session.result.primary_cmm==3200&&page.session.result.secondary_cmm==2000);
    screenshot("rectangle.ppm");
    /* 超过容量后只保留64条，累计统计不因滚动淘汰而减少。 */
    for(unsigned i=0;i<70;++i)CHECK(instrument_add_record(&model,&page.session.result));
    CHECK(model.record_count==64&&instrument_model_check(&model));
    dashboard_open(&ui,DASH_RECORDS);advance(250);CHECK(lv_obj_get_scroll_bottom(ui.body)>0);
    /* 手指拖动滚动记录列表，检查末尾有入口继续加载下一段。 */
    tx=260;ty=245;down=true;advance(40);
    for(int y=245;y>=90;y-=10){ty=y;advance(20);}down=false;advance(200);CHECK(lv_obj_get_scroll_y(ui.body)>0);
    screenshot("records_scroll.ppm");dashboard_close(&ui);advance(250);
    /* 重复打开不同二级页，检查销毁/重建不会把48KB池耗尽。 */
    for(unsigned i=0;i<30;++i){dashboard_open(&ui,(DashboardMenu)(i%7));advance(200);dashboard_close(&ui);advance(200);CHECK(lv_mem_test()==LV_RESULT_OK);}
    MeasurementBackend original=page.backend,quiet={.start=silent_backend};measurement_page_set_backend(&page,&quiet);
    measurement_page_navigate(&page,1);page.session.timeout_ms=2000;uint32_t count=model.inspected;
    dashboard_action(&ui);CHECK(page.automatic);advance(2300);
    CHECK(page.session.state==MEASUREMENT_FAILED&&!page.automatic&&model.inspected==count);
    measurement_page_set_backend(&page,&original);
    lv_mem_monitor_t monitor;lv_mem_monitor(&monitor);
    printf("PASS: modes, bottom touch, auto inspection, tolerance, records, scroll, rollback, Wi-Fi contracts, rectangle; heap free=%lu max_used=%lu\n",(unsigned long)monitor.free_size,(unsigned long)monitor.max_used);
    dashboard_destroy(&ui);return 0;
}
