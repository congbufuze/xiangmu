#include "AppServices.h"
#include "AppPower.h"
#include "lcd_init.h"
#include "SystemStartup.h"
#include "HWDataAccess.h"
#include "main.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui.h"
#include "ui_HomePage.h"
#include "ui_MenuPage.h"
#include "ui_SetPage.h"
#include "ui_OffTimePage.h"
static void sync_settings(const AppSettings *settings)
{
    ui_LightSliderValue = settings->brightness;
    ui_LTimeValue = settings->idle_seconds;
    ui_TTimeValue = settings->sleep_seconds;
    HWInterface.IMU.wrist_is_enabled = settings->wrist_enabled;
    for (unsigned i = 0; i < 6; ++i) {
        if (ui_LTimeOptions[i] == settings->idle_seconds) ui_LTimeSelected = i;
        if (ui_TTimeOptions[i] == settings->sleep_seconds) ui_TTimeSelected = i;
    }
}
static void sync_sensors(void)
{
    AppSensorSnapshot s;
    AppSensors_Read(&s);
    /* UI task owns these legacy page models. */
    HWInterface.AHT21.temperature = s.temperature;
    HWInterface.AHT21.humidity = s.humidity;
    HWInterface.IMU.Steps = s.steps;
    HWInterface.Power.power_remain = s.battery;
    HWInterface.HR_meter.HrRate = s.heart_rate;
    HWInterface.HR_meter.SPO2 = s.spo2;
    HWInterface.Ecompass.direction = s.direction;
    HWInterface.Barometer.altitude = s.altitude;
}
void StartUITask(void *argument)
{
    (void)argument;
    SystemStartup_WaitReady();
    AppSettings settings;
    AppSettings_Read(&settings);
    sync_settings(&settings);
    sync_sensors();
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();
    lv_port_indev_init();
    ui_init();
    ScreenPower power;
    ScreenPower_Init(&power, HAL_GetTick());
    uint8_t applied_light = 255;
    uint32_t wait_ticks = 1U;
    for (;;) {
        uint32_t flags = osThreadFlagsWait(APP_UI_NOTIFY | APP_UI_TOUCH_IRQ,
                                           osFlagsWaitAny, wait_ticks);
        uint32_t events = App_TakeEvents();
        if (!(flags & osFlagsError) && (flags & APP_UI_TOUCH_IRQ)) {
            bool sleeping = power.state == SCREEN_SLEEPING;
            lv_port_indev_notify_touch(sleeping);
            if (sleeping) events |= APP_EVENT_WAKE_REQUEST;
        }
        if (events & APP_EVENT_SETTINGS) {
            AppSettings_Read(&settings);
            sync_settings(&settings);
        }
        if (events & APP_EVENT_SENSOR_UPDATED) sync_sensors();
        Page_t *page = Page_Get_NowPage();
        if ((events & APP_EVENT_WRIST_DOWN) && page &&
            (page->page_obj == &ui_HomePage || page->page_obj == &ui_MenuPage || page->page_obj == &ui_SetPage))
            events |= APP_EVENT_SLEEP_REQUEST;
        uint8_t command;
        bool waking = power.state == SCREEN_SLEEPING;
        while (osMessageQueueGet(KEYQueueHandle, &command, NULL, 0) == osOK) {
            if (waking) { events |= APP_EVENT_ACTIVITY; continue; }
            if (command == APP_KEY_BACK) { Page_Back(); events |= APP_EVENT_ACTIVITY; }
            else if (command == APP_KEY_HOME_OR_SLEEP) {
                page = Page_Get_NowPage();
                if (page && page->page_obj == &ui_HomePage) events |= APP_EVENT_SLEEP_REQUEST;
                else { Page_Back_Bottom(); events |= APP_EVENT_ACTIVITY; }
            }
        }
        ScreenState previous = power.state;
        ScreenPower_Step(&power, HAL_GetTick(), events, &settings);
        g_app_diagnostics.screen_state = power.state;
        lv_timer_t *refresh = lv_display_get_refr_timer(lv_display_get_default());
        if (previous != power.state) {
            if (power.state == SCREEN_SLEEPING) {
                lv_display_enable_invalidation(lv_display_get_default(), false);
                lv_timer_pause(refresh);
                HWInterface.LCD.SetLight(0);
                applied_light = 0;
                LCD_ST7789_SleepIn();
                ++g_app_diagnostics.lcd_sleep_count;
            }
            else if (previous == SCREEN_SLEEPING) {
                LCD_ST7789_SleepOut();
                ++g_app_diagnostics.lcd_wake_count;
                sync_sensors();
                lv_port_indev_notify_touch(true);
                lv_display_enable_invalidation(lv_display_get_default(), true);
                lv_timer_resume(refresh);
                lv_obj_invalidate(lv_screen_active());
                lv_timer_ready(refresh);
            }
        }
        if (power.state == SCREEN_SLEEPING) {
            /* Pause all LVGL processing, not just drawing. Only an input,
             * settings or explicit power event needs to wake the UI task. */
            wait_ticks = osWaitForever;
            continue;
        }
        uint32_t delay = lv_timer_handler();
        uint8_t light = power.state == SCREEN_SLEEPING ? 0 :
            power.state == SCREEN_DIMMED ? (settings.brightness > 10 ? 10 : settings.brightness) : settings.brightness;
        if (light != applied_light) { HWInterface.LCD.SetLight(light); applied_light = light; }
        if (delay > 10) delay = 10;
        wait_ticks = App_MsToTicks(delay ? delay : 1);
    }
}
