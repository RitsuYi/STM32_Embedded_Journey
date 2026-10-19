cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 03_F407_HAL_FreeRTOS)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# LVGL configuration for this project
set(LV_BUILD_CONF_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
    CACHE FILEPATH "LVGL configuration header"
    FORCE
)

# Keep the CMake targets consistent with the embedded lv_conf.h.
# LVGL v9.6.0 defaults these build options to ON independently of lv_conf.h.
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable LVGL demos" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable LVGL examples" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "Disable ThorVG" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Disable internal ThorVG" FORCE)

add_subdirectory(LVGL)
# 仅F407固件启用CCM池；主机回归仍由LVGL自行分配同容量内存，不能访问MCU地址。
target_compile_definitions(lvgl PUBLIC GUI_USE_CCM_POOL=1)
target_include_directories(lvgl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/User/GUI")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Collect sources and headers in User and all of its subdirectories
file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/User/*.c"
)
file(GLOB_RECURSE USER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/User/*.h"
)

# Add every directory containing a User header to the header search path
set(USER_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/User")
foreach(USER_HEADER IN LISTS USER_HEADERS)
    get_filename_component(USER_HEADER_DIR "${USER_HEADER}" DIRECTORY)
    list(APPEND USER_INCLUDE_DIRS "${USER_HEADER_DIR}")
endforeach()
list(REMOVE_DUPLICATES USER_INCLUDE_DIRS)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_DRIVER_SOURCES}
    ${USER_DRIVER_HEADERS}
    ${USER_SOURCES}
    ${USER_HEADERS}
    FreeRTOS/SRC/croutine.c
    FreeRTOS/SRC/event_groups.c
    FreeRTOS/SRC/heap_4.c
    FreeRTOS/SRC/list.c
    FreeRTOS/SRC/port.c
    FreeRTOS/SRC/queue.c
    FreeRTOS/SRC/stream_buffer.c
    FreeRTOS/SRC/tasks.c
    FreeRTOS/SRC/timers.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Drivers
    FreeRTOS/INC
    ${USER_INCLUDE_DIRS}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
    # Add user defined libraries
)

# Standard C snprintf fractional formatting is separate from LVGL's LV_USE_FLOAT.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)
file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/User/GUI/*.c")
set_source_files_properties(${GUI_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wextra;-Wformat=2")
