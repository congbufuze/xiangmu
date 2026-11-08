#include "lv_port_disp.h"

#include "lcd.h"
#include "lcd_init.h"

#define LV_PORT_DISP_BUFFER_LINES 20U

static lv_display_t * s_flushing_display;
static uint16_t s_draw_buffer[LCD_W * LV_PORT_DISP_BUFFER_LINES];

static void disp_flush(lv_display_t * display, const lv_area_t * area, uint8_t * pixel_map)
{
    s_flushing_display = display;
    LCD_Color_Fill((uint16_t)area->x1,
                   (uint16_t)area->y1,
                   (uint16_t)area->x2,
                   (uint16_t)area->y2,
                   (uint16_t *)pixel_map);
}

void lcd_flush_ready_callback(void)
{
    if (s_flushing_display != NULL) {
        lv_display_flush_ready(s_flushing_display);
        s_flushing_display = NULL;
    }
}

void lv_port_disp_init(void)
{
    lv_display_t * display = lv_display_create(LCD_W, LCD_H);
    if (display == NULL) {
        return;
    }

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, disp_flush);
    lv_display_set_buffers(display,
                           s_draw_buffer,
                           NULL,
                           sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    LCD_Set_Flush_Complete_Callback(lcd_flush_ready_callback);
}
