cmake_minimum_required(VERSION 3.22)
project(ui_dashboard C CXX)
set(CMAKE_C_STANDARD 11)
get_filename_component(F407_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${F407_ROOT}/lv_conf.h" CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${F407_ROOT}/LVGL" lvgl)
target_include_directories(lvgl PUBLIC "${F407_ROOT}")
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS "${F407_ROOT}/User/GUI/measurement/*.c" "${F407_ROOT}/User/GUI/fonts/*.c"
    "${F407_ROOT}/User/GUI/assets/*.c")
add_executable(ui_dashboard_test dashboard_test.c ${GUI_SOURCES}
    "${F407_ROOT}/User/GUI/instrument/instrument_model.c"
    "${F407_ROOT}/User/GUI/instrument/dashboard_ui.c"
    "${F407_ROOT}/User/GUI/gui_demo.c" "${F407_ROOT}/User/GUI/gui_app.c"
    "${F407_ROOT}/User/GUI/lvgl_port.c")
target_include_directories(ui_dashboard_test PRIVATE "${F407_ROOT}/User/GUI"
    "${F407_ROOT}/tests/gui_migration/touch_stubs" "${F407_ROOT}/tests/gui_migration/stubs"
    "${F407_ROOT}/User/Hardware")
target_compile_options(ui_dashboard_test PRIVATE -Wall -Wextra -Werror -Wformat=2)
target_link_libraries(ui_dashboard_test PRIVATE lvgl m)
enable_testing()
add_test(NAME ui_dashboard COMMAND ui_dashboard_test)

# 同轮RGB565排障直接编译实际LCD驱动；仅HAL与DMA等待由主机替身提供。
add_executable(rgb565_transfer_test "${F407_ROOT}/tests/gui_migration/st7796_transfer_test.c"
    "${F407_ROOT}/User/Hardware/ST7796.c")
target_include_directories(rgb565_transfer_test PRIVATE "${F407_ROOT}/tests/gui_migration/stubs"
    "${F407_ROOT}/User/Hardware")
target_compile_options(rgb565_transfer_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME rgb565_transfer COMMAND rgb565_transfer_test)
