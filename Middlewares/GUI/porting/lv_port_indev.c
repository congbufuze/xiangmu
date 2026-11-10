#include "lv_port_indev.h"

#include "CST816.h"
#include "AppServices.h"

extern CST816_Info CST816_Instance;

static lv_indev_t * s_touchpad;
static bool suppress_until_release;

void lv_port_indev_notify_touch(bool waking)
{
    if (waking) suppress_until_release = true;
    if (s_touchpad) lv_timer_ready(lv_indev_get_read_timer(s_touchpad));
}

static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;

    uint8_t finger_count;
    uint16_t x;
    uint16_t y;
    if (CST816_ReadTouch(&finger_count, &x, &y) != 0U) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    if (finger_count == 0) suppress_until_release = false;
    if (finger_count == 1U) {
        App_PostEvent(APP_EVENT_ACTIVITY);
        if (AppScreen_IsSleeping()) suppress_until_release = true;
        if (suppress_until_release) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_indev_init(void)
{
    s_touchpad = lv_indev_create();
    if (s_touchpad == NULL) {
        return;
    }

    lv_indev_set_type(s_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touchpad, touchpad_read);
}
