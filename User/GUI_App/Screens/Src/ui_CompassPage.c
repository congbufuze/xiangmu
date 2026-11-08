#include "../../ui.h"
#include "../../ui_helpers.h"
#include "../Inc/ui_CompassPage.h"
#include "../../../Func/Inc/HWDataAccess.h"

///////////////////// Page Manager //////////////////
Page_t Page_Compass = {ui_CompassPage_screen_init, ui_CompassPage_screen_deinit, &ui_CompassPage};

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_CompassPage;
lv_obj_t * ui_CompassMeter;
static lv_obj_t * ui_CompassScale360;
static lv_obj_t * ui_CompassScale12;
lv_obj_t * ui_Compassneedle;

lv_obj_t * ui_CompassDirBtn;
lv_obj_t * ui_CompassDirLabel;
lv_obj_t * ui_CompassDirIcon;

lv_obj_t * ui_EnvAltitudeBtn;
lv_obj_t * ui_EnvAltitudeLabel;
lv_obj_t * ui_EnvAltitudeIcon;

lv_timer_t * ui_EcompassPageTimer;

///////////////////// FUNCTIONS ////////////////////


/////////////////// private Timer ///////////////////
// need to be destroyed when the page is destroyed
static void EcompassPage_timer_cb(lv_timer_t * timer)
{
    uint8_t value_strbuf[6];
    //set text
    lv_img_set_angle(ui_Compassneedle, HWInterface.Ecompass.direction * 10);
    sprintf(value_strbuf,":%d", HWInterface.Ecompass.direction);
    lv_label_set_text(ui_CompassDirLabel, value_strbuf);

    //set text
    sprintf(value_strbuf,":%dm", HWInterface.Barometer.altitude);
    lv_label_set_text(ui_EnvAltitudeLabel, value_strbuf);
}

///////////////////// SCREEN init ////////////////////
void ui_CompassPage_screen_init(void)
{
    uint8_t value_strbuf[10];

		ui_CompassPage = lv_obj_create(NULL);

    ui_CompassMeter = lv_obj_create(ui_CompassPage);
    lv_obj_set_size(ui_CompassMeter, 230, 230);
    lv_obj_center(ui_CompassMeter);
    lv_obj_set_pos(ui_CompassMeter,0,-20);
    lv_obj_remove_style_all(ui_CompassMeter);
    lv_obj_remove_flag(ui_CompassMeter, LV_OBJ_FLAG_SCROLLABLE);

    ui_CompassScale360 = lv_scale_create(ui_CompassMeter);
    lv_obj_set_size(ui_CompassScale360, 230, 230);
    lv_obj_center(ui_CompassScale360);
    lv_scale_set_mode(ui_CompassScale360, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_total_tick_count(ui_CompassScale360, 61);
    lv_scale_set_major_tick_every(ui_CompassScale360, 1);
    lv_scale_set_label_show(ui_CompassScale360, false);
    lv_scale_set_range(ui_CompassScale360, 0, 360);
    lv_scale_set_angle_range(ui_CompassScale360, 360);
    lv_scale_set_rotation(ui_CompassScale360, 0);
    lv_obj_set_style_bg_opa(ui_CompassScale360, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_CompassScale360, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_CompassScale360, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(ui_CompassScale360, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_length(ui_CompassScale360, 10, LV_PART_INDICATOR);

    ui_CompassScale12 = lv_scale_create(ui_CompassMeter);
    lv_obj_set_size(ui_CompassScale12, 230, 230);
    lv_obj_center(ui_CompassScale12);
    lv_scale_set_mode(ui_CompassScale12, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_total_tick_count(ui_CompassScale12, 12);
    lv_scale_set_major_tick_every(ui_CompassScale12, 1);
    lv_scale_set_label_show(ui_CompassScale12, true);
    lv_scale_set_range(ui_CompassScale12, 0, 330);
    lv_scale_set_angle_range(ui_CompassScale12, 330);
    lv_scale_set_rotation(ui_CompassScale12, 270);
    lv_obj_set_style_bg_opa(ui_CompassScale12, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_CompassScale12, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_CompassScale12, 2, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(ui_CompassScale12, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_length(ui_CompassScale12, 15, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(ui_CompassScale12, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(ui_CompassScale12, 15, LV_PART_INDICATOR);


    ui_Compassneedle = lv_img_create(ui_CompassMeter);
    lv_img_set_src(ui_Compassneedle, &ui_img_compass_needle_png);
    lv_obj_set_width(ui_Compassneedle, LV_SIZE_CONTENT);   /// 23
    lv_obj_set_height(ui_Compassneedle, LV_SIZE_CONTENT);    /// 171
    lv_obj_set_align(ui_Compassneedle, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Compassneedle, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_remove_flag(ui_Compassneedle, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_img_set_angle(ui_Compassneedle, HWInterface.Ecompass.direction * 10);


    ui_EnvAltitudeBtn = lv_btn_create(ui_CompassPage);
    lv_obj_set_size(ui_EnvAltitudeBtn, 100, 30);
    lv_obj_align_to(ui_EnvAltitudeBtn,ui_CompassMeter,LV_ALIGN_BOTTOM_MID,60,50);
    lv_obj_add_flag(ui_EnvAltitudeBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_EnvAltitudeBtn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_EnvAltitudeBtn, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_EnvAltitudeBtn, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_EnvAltitudeBtn, 128, LV_PART_MAIN);

    ui_EnvAltitudeIcon = lv_label_create(ui_EnvAltitudeBtn);
    lv_obj_set_align(ui_EnvAltitudeIcon,LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(ui_EnvAltitudeIcon,-10,0);
    lv_label_set_text(ui_EnvAltitudeIcon, "\xEE\x9A\x9B");
    lv_obj_set_style_text_opa(ui_EnvAltitudeIcon, 255, LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_EnvAltitudeIcon, &ui_font_iconfont28, LV_PART_MAIN);

    ui_EnvAltitudeLabel = lv_label_create(ui_EnvAltitudeBtn);
    lv_obj_set_align(ui_EnvAltitudeLabel,LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(ui_EnvAltitudeLabel,17,0);
		sprintf(value_strbuf,":%dm", HWInterface.Barometer.altitude);
    lv_label_set_text(ui_EnvAltitudeLabel, value_strbuf);
    lv_obj_set_style_text_font(ui_EnvAltitudeLabel, &ui_font_Cuyuan20, LV_PART_MAIN);

    ui_CompassDirBtn = lv_btn_create(ui_CompassPage);
    lv_obj_set_size(ui_CompassDirBtn, 100, 30);
    lv_obj_align_to(ui_CompassDirBtn,ui_CompassMeter,LV_ALIGN_BOTTOM_MID,-60,50);
    lv_obj_add_flag(ui_CompassDirBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_CompassDirBtn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_CompassDirBtn, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_CompassDirBtn, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_CompassDirBtn, 128, LV_PART_MAIN);

    ui_CompassDirIcon = lv_label_create(ui_CompassDirBtn);
    lv_obj_set_align(ui_CompassDirIcon,LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(ui_CompassDirIcon,-10,0);
    lv_label_set_text(ui_CompassDirIcon, "\xee\x9e\x88");
    lv_obj_set_style_text_opa(ui_CompassDirIcon, 255, LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_CompassDirIcon, &ui_font_iconfont28, LV_PART_MAIN);

    ui_CompassDirLabel = lv_label_create(ui_CompassDirBtn);
    lv_obj_set_align(ui_CompassDirLabel,LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(ui_CompassDirLabel,17,0);
		sprintf(value_strbuf,":%d", HWInterface.Ecompass.direction);
    lv_label_set_text(ui_CompassDirLabel, value_strbuf);
    lv_obj_set_style_text_font(ui_CompassDirLabel, &ui_font_Cuyuan20, LV_PART_MAIN);

    //timer
    ui_EcompassPageTimer = lv_timer_create(EcompassPage_timer_cb, 500,  NULL);

}

/////////////////// SCREEN deinit ////////////////////
void ui_CompassPage_screen_deinit(void)
{
  lv_timer_delete(ui_EcompassPageTimer);
}
