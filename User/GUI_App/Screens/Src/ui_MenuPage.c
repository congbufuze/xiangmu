#include "../Inc/ui_MenuPage.h"
#include "../Inc/ui_CalendarPage.h"
#include "../Inc/ui_ComputerPage.h"
#include "../Inc/ui_EnvPage.h"
#include "../Inc/ui_GameSelectPage.h"
#include "../Inc/ui_SetPage.h"
#include "../Inc/ui_AboutPage.h"

lv_obj_t * ui_MenuPage;
int16_t ui_MenuScrollY;
Page_t Page_Menu = {ui_MenuPage_screen_init, ui_MenuPage_screen_deinit, &ui_MenuPage};

typedef struct {
    const char *text;
    const char *icon;
    const lv_font_t *font;
    uint32_t color;
    Page_t *page;
} MenuItem;

static const MenuItem menu_items[] = {
    {"日 历", "\xEE\x98\x81", &ui_font_iconfont30, 0xFF8080, &Page_Calender},
    {"计 算 器", "\xEE\xA2\x9B", &ui_font_iconfont28, 0xC86400, &Page_Computer},
    {"环 境", "\xEE\x9C\x86", &ui_font_iconfont34, 0x009632, &Page_Env},
    {"游 戏", "\xEE\x98\xB5", &ui_font_iconfont30, 0xC07010, &Page_GameSelect},
    {"设 置", "\xEE\x98\x80", &ui_font_iconfont30, 0x808080, &Page_Set},
    {"关 于", "\xEE\x98\x8B", &ui_font_iconfont30, 0x646464, &Page_About},
};

static void menu_event(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_GESTURE) {
        if(lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) Page_Back();
    }
    else if(lv_event_get_code(e) == LV_EVENT_SCROLL_END) {
        ui_MenuScrollY = lv_obj_get_scroll_y(ui_MenuPage);
    }
}

static void item_clicked(lv_event_t *e)
{
    Page_Load(lv_event_get_user_data(e));
}

static void create_item(const MenuItem *item, unsigned index)
{
    lv_obj_t *panel = lv_obj_create(ui_MenuPage);
    lv_obj_set_size(panel, 240, 70);
    lv_obj_set_align(panel, LV_ALIGN_TOP_MID);
    lv_obj_set_y(panel, index * 70);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x808080), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(panel, 100, LV_STATE_PRESSED);
    lv_obj_add_event_cb(panel, item_clicked, LV_EVENT_CLICKED, item->page);

    lv_obj_t *button = lv_button_create(panel);
    lv_obj_set_size(button, 40, 40);
    lv_obj_set_align(button, LV_ALIGN_LEFT_MID);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(button, 40, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(item->color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *icon = lv_label_create(button);
    lv_obj_set_align(icon, LV_ALIGN_CENTER);
    lv_label_set_text_static(icon, item->icon);
    lv_obj_set_style_text_font(icon, item->font, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(panel);
    lv_obj_set_align(label, LV_ALIGN_LEFT_MID);
    lv_obj_set_x(label, 60);
    lv_label_set_text_static(label, item->text);
    lv_obj_set_style_text_font(label, &ui_font_Cuyuan20, LV_PART_MAIN);
}

void ui_MenuPage_screen_init(void)
{
    ui_MenuPage = lv_obj_create(NULL);
    for(unsigned i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); ++i) {
        create_item(&menu_items[i], i);
    }
    lv_obj_update_layout(ui_MenuPage);
    lv_obj_scroll_to(ui_MenuPage, 0, ui_MenuScrollY, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_MenuPage, menu_event, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_MenuPage, menu_event, LV_EVENT_SCROLL_END, NULL);
}

void ui_MenuPage_screen_deinit(void)
{
    /* PageManager owns screen deletion through the transition animation. */
}
