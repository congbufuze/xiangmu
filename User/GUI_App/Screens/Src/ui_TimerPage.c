#include "../../ui.h"
#include "../../ui_helpers.h"
#include "../Inc/ui_TimerPage.h"

///////////////////// Page Manager //////////////////
Page_t Page_Timer = {ui_TimerPage_screen_init, ui_TimerPage_screen_deinit, &ui_TimerPage};

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_TimerPage;
lv_obj_t * ui_TimerMeter;
lv_obj_t * ui_TimerStartBtn;
lv_obj_t * ui_TimerReBtn;
lv_obj_t * ui_TimerStartLabel;
lv_obj_t * ui_TimerReLabel;
lv_obj_t * ui_TimerMinLabel;
lv_obj_t * ui_TimerSecColonLabel;
lv_obj_t * ui_TimerSecLabel;
lv_obj_t * ui_TimerMilSecColonLabel;
lv_obj_t * ui_TimerMilSecLabel;

lv_timer_t * ui_TimerPageTimer;

uint8_t ui_TimerPageFlag=0;
uint8_t ui_TimerPage_min=0;
uint8_t ui_TimerPage_sec=0;
uint8_t ui_TimerPage_10ms=0;
uint8_t ui_TimerPage_ms=0;

///////////////////// ANIMATIONS ////////////////////


///////////////////// Data Init ////////////////////
static void ui_TimerPage_Data_init(void)
{
    ui_TimerPageFlag=0;
    ui_TimerPage_min=0;
    ui_TimerPage_sec=0;
	ui_TimerPage_10ms=0;
    ui_TimerPage_ms=0;
}

/////////////////////// Timer //////////////////////
static void LabelRefresh_timer(lv_timer_t * timer)
{
    uint8_t strbuf[2];

    sprintf(strbuf,"%02d",ui_TimerPage_min);
    lv_label_set_text(ui_TimerMinLabel, strbuf);
    sprintf(strbuf,"%02d",ui_TimerPage_sec);
    lv_label_set_text(ui_TimerSecLabel, strbuf);
    sprintf(strbuf,"%02d",ui_TimerPage_10ms);
    lv_label_set_text(ui_TimerMilSecLabel, strbuf);


}

///////////////////// FUNCTIONS ////////////////////
static void ui_event_TimerStartBtn(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_VALUE_CHANGED &&  lv_obj_has_state(target, LV_STATE_CHECKED))
    {
        //checked
        ui_TimerPageFlag = 1;
        lv_label_set_text(ui_TimerStartLabel, LV_SYMBOL_PAUSE);
        lv_timer_resume(ui_TimerPageTimer);

    }
    if(event_code == LV_EVENT_VALUE_CHANGED &&  !lv_obj_has_state(target, LV_STATE_CHECKED))
    {
        //released
        ui_TimerPageFlag = 0;
        lv_label_set_text(ui_TimerStartLabel, LV_SYMBOL_PLAY);
        lv_timer_pause(ui_TimerPageTimer);

    }
}

static void ui_event_TimerReBtn(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED && !lv_obj_has_state(ui_TimerStartBtn, LV_STATE_CHECKED))
    {
        //clear
        lv_timer_pause(ui_TimerPageTimer);
        lv_label_set_text(ui_TimerMinLabel, "00");
        lv_label_set_text(ui_TimerSecLabel, "00");
        lv_label_set_text(ui_TimerMilSecLabel, "00");
        ui_TimerPage_min = 0;
        ui_TimerPage_sec = 0;
		ui_TimerPage_10ms = 0;
        ui_TimerPage_ms = 0;

    }

}

///////////////////// SCREEN init ////////////////////
void ui_TimerPage_screen_init(void)
{

    ui_TimerPage_Data_init();
    ui_TimerPage = lv_obj_create(NULL);

    ui_TimerMeter = lv_scale_create(ui_TimerPage);
    lv_obj_set_size(ui_TimerMeter, 240, 240);
    lv_obj_center(ui_TimerMeter);
    lv_scale_set_mode(ui_TimerMeter, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_total_tick_count(ui_TimerMeter, 61);
    lv_scale_set_major_tick_every(ui_TimerMeter, 1);
    lv_scale_set_label_show(ui_TimerMeter, false);
    lv_scale_set_range(ui_TimerMeter, 0, 60);
    lv_scale_set_angle_range(ui_TimerMeter, 360);
    lv_scale_set_rotation(ui_TimerMeter, 270);
    lv_obj_set_style_bg_opa(ui_TimerMeter, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_TimerMeter, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_TimerMeter, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(ui_TimerMeter, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_length(ui_TimerMeter, 10, LV_PART_INDICATOR);

    ui_TimerStartBtn = lv_btn_create(ui_TimerMeter);
    lv_obj_set_width(ui_TimerStartBtn, 60);
    lv_obj_set_height(ui_TimerStartBtn, 60);
    lv_obj_set_style_radius(ui_TimerStartBtn, 50, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_TimerStartBtn, 200, LV_PART_MAIN);
    lv_obj_set_align(ui_TimerStartBtn, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerStartBtn,40,50);
    lv_obj_add_flag(ui_TimerStartBtn, LV_OBJ_FLAG_CHECKABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_TimerStartBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags


    ui_TimerReBtn = lv_btn_create(ui_TimerMeter);
    lv_obj_set_width(ui_TimerReBtn, 60);
    lv_obj_set_height(ui_TimerReBtn, 60);
    lv_obj_set_style_radius(ui_TimerReBtn, 50, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_TimerReBtn, 200, LV_PART_MAIN);
    lv_obj_set_align(ui_TimerReBtn, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerReBtn,-40,50);

    ui_TimerStartLabel = lv_label_create(ui_TimerStartBtn);
    lv_label_set_text(ui_TimerStartLabel, LV_SYMBOL_PLAY);//LV_SYMBOL_PAUSE
    lv_obj_center(ui_TimerStartLabel);
    lv_obj_set_style_text_font(ui_TimerStartLabel, &lv_font_montserrat_24, LV_PART_MAIN);

    ui_TimerReLabel = lv_label_create(ui_TimerReBtn);
    lv_label_set_text(ui_TimerReLabel, LV_SYMBOL_REFRESH);
    lv_obj_center(ui_TimerReLabel);
    lv_obj_set_style_text_font(ui_TimerReLabel, &lv_font_montserrat_24, LV_PART_MAIN);

    ui_TimerMinLabel = lv_label_create(ui_TimerMeter);
    lv_obj_set_align(ui_TimerMinLabel, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerMinLabel,-60,-20);
    lv_label_set_text(ui_TimerMinLabel, "00");
    lv_obj_set_style_text_font(ui_TimerMinLabel, &ui_font_Cuyuan38, LV_PART_MAIN);

    ui_TimerSecColonLabel = lv_label_create(ui_TimerMeter);
    lv_obj_set_align(ui_TimerSecColonLabel, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerSecColonLabel,-30,-20);
    lv_label_set_text(ui_TimerSecColonLabel, ":");
    lv_obj_set_style_text_font(ui_TimerSecColonLabel, &ui_font_Cuyuan38, LV_PART_MAIN);

    ui_TimerSecLabel = lv_label_create(ui_TimerMeter);
    lv_obj_set_align(ui_TimerSecLabel, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerSecLabel,0,-20);
    lv_label_set_text(ui_TimerSecLabel, "00");
    lv_obj_set_style_text_font(ui_TimerSecLabel, &ui_font_Cuyuan38, LV_PART_MAIN);

    ui_TimerMilSecColonLabel = lv_label_create(ui_TimerMeter);
    lv_obj_set_align(ui_TimerMilSecColonLabel, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerMilSecColonLabel,30,-20);
    lv_label_set_text(ui_TimerMilSecColonLabel, ".");
    lv_obj_set_style_text_font(ui_TimerMilSecColonLabel, &ui_font_Cuyuan38, LV_PART_MAIN);

    ui_TimerMilSecLabel = lv_label_create(ui_TimerMeter);
    lv_obj_set_align(ui_TimerMilSecLabel, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_TimerMilSecLabel,60,-20);
    lv_label_set_text(ui_TimerMilSecLabel, "00");
    lv_obj_set_style_text_font(ui_TimerMilSecLabel, &ui_font_Cuyuan38, LV_PART_MAIN);

    //events
    lv_obj_add_event_cb(ui_TimerStartBtn, ui_event_TimerStartBtn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_TimerReBtn, ui_event_TimerReBtn, LV_EVENT_ALL, NULL);

    //timer
    ui_TimerPageTimer = lv_timer_create(LabelRefresh_timer, 10,  NULL);
    lv_timer_pause(ui_TimerPageTimer);

}

//////////////////// SCREEN deinit ////////////////////
void ui_TimerPage_screen_deinit(void)
{}
