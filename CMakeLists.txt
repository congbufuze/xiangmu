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
set(CMAKE_PROJECT_NAME watch)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
add_compile_options($<$<COMPILE_LANGUAGE:C>:-flto>)
add_link_options(-flto)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
    User/Task/Inc/UITask.h
    User/Task/Inc/KeyTask.h
    User/Task/Inc/HardwareInitTask.h
    User/Task/Src/AHT21Task.c
    User/Task/Src/HardwareInit.c
    User/Task/Src/KeyTask.c
    User/Task/Src/UITask.c
    User/Task/Src/DataSaveTask.c)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)
# The Cortex-M port calls kernel symbols from inline assembly; keep their
# externally visible definitions intact while optimizing the application/UI.
target_compile_options(FreeRTOS PRIVATE -fno-lto)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    BSP/AHT21/AHT21.c
    BSP/IIC/iic_hal.c
    BSP/KEY/key.c
    BSP/LCD/lcd.c
    BSP/LCD/lcd_init.c
    BSP/MPU6050/mpu6050.c
    BSP/MPU6050/eMPL/inv_mpu.c
    BSP/MPU6050/eMPL/inv_mpu_dmp_motion_driver.c
    BSP/OWDG/WDOG.c
    BSP/POWER/power.c
    BSP/TOUCH/CST816.c
    SYSTEM/delay.c
    User/Func/Src/AppTypes.c
    User/Func/Src/ScreenPower.c
    User/Func/Src/AppServices.c
    User/Func/Src/AppPower.c
    User/Func/Src/SettingsJournal.c
    User/Func/Src/SettingsFlash.c
    User/Func/Src/HWDataAccess.c
    User/Func/Src/PageManager.c
    User/Func/Src/pubsub.c
    User/Func/Src/StrCalculate.c
)

# LVGL library sources
file(GLOB_RECURSE LVGL_SOURCES
    Middlewares/GUI/lvgl/src/*.c
)
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/(uefi|rtthread|micropython|windows|sdl2|mqx|linux)/.*")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${LVGL_SOURCES})

# LVGL porting sources
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Middlewares/GUI/porting/lv_port_disp.c
    Middlewares/GUI/porting/lv_port_indev.c
)

# GUI application sources
file(GLOB_RECURSE GUI_SOURCES
    User/GUI_App/*.c
    User/GUI_App/Screens/Src/*.c
    User/GUI_App/Fonts/*.c
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${GUI_SOURCES})

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    BSP/AHT21
    BSP/IIC
    BSP/KEY
    BSP/LCD
    BSP/MPU6050
    BSP/MPU6050/eMPL
    BSP/OWDG
    BSP/POWER
    BSP/TOUCH
    SYSTEM
    User/Task/Inc
    User/Task/Src
    User/Func/Inc
    # LVGL
    Middlewares/GUI
    Middlewares/GUI/lvgl
    Middlewares/GUI/lvgl/include
    Middlewares/GUI/porting
    # GUI application
    User/GUI_App
    User/GUI_App/Screens/Inc
    User/GUI_App/Fonts
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

    # Add user defined libraries
)
