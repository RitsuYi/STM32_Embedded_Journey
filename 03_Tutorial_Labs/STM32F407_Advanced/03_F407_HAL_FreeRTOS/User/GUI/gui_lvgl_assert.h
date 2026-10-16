#ifndef GUI_LVGL_ASSERT_H
#define GUI_LVGL_ASSERT_H
#ifdef __cplusplus
extern "C" {
#endif
void GUI_LVGL_AssertFailed(const char *file, int line);
#ifdef __cplusplus
}
#endif
#define LV_ASSERT_HANDLER GUI_LVGL_AssertFailed(__FILE__, __LINE__);
#endif
