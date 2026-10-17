cmake_minimum_required(VERSION 3.22)
project(gui_migration_test C CXX)
set(CMAKE_C_STANDARD 11)
get_filename_component(F407_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${F407_ROOT}/lv_conf.h" CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${F407_ROOT}/LVGL" lvgl)
target_include_directories(lvgl PUBLIC "${F407_ROOT}")
file(GLOB_RECURSE APP_SOURCES
    "${F407_ROOT}/User/GUI/measurement/*.c"
    "${F407_ROOT}/User/GUI/instrument/*.c"
    "${F407_ROOT}/User/GUI/fonts/*.c")
add_executable(gui_migration_test gui_migration_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/gui_demo.c" "${F407_ROOT}/User/GUI/gui_app.c")
target_include_directories(gui_migration_test PRIVATE "${F407_ROOT}/User/GUI")
target_compile_options(gui_migration_test PRIVATE -Wall -Wextra -Wformat=2)
target_link_libraries(gui_migration_test PRIVATE lvgl m)
enable_testing()
add_test(NAME gui_migration COMMAND gui_migration_test)
add_executable(st7796_transfer_test st7796_transfer_test.c "${F407_ROOT}/User/Hardware/ST7796.c")
target_include_directories(st7796_transfer_test PRIVATE stubs "${F407_ROOT}/User/Hardware")
target_compile_options(st7796_transfer_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME st7796_transfer COMMAND st7796_transfer_test)

# H6.5回归：通过实际输入适配和LVGL命中检测点击按钮，不直接伪造CLICKED事件。
add_executable(gui_touch_test gui_touch_test.c ${APP_SOURCES}
    "${F407_ROOT}/User/GUI/gui_demo.c" "${F407_ROOT}/User/GUI/lvgl_port.c")
target_include_directories(gui_touch_test PRIVATE touch_stubs stubs
    "${F407_ROOT}/User/GUI" "${F407_ROOT}/User/Hardware")
target_compile_options(gui_touch_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(gui_touch_test PRIVATE lvgl m)
add_test(NAME gui_touch COMMAND gui_touch_test)
