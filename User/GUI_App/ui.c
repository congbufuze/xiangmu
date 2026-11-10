// LVGL VERSION: 9.6.0

#include "ui.h"
#include "./Screens/Inc/ui_HomePage.h"
#include "./Screens/Inc/ui_MenuPage.h"

///////////////////// TEST LVGL SETTINGS ////////////////////

#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif

/////////////////////// ui_initialize //////////////////////
void ui_init(void)
{

    lv_display_t * dispp = lv_display_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               true, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    Pages_init();
}