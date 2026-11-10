#ifndef _UI_MENUPAGE_H
#define _UI_MENUPAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../ui.h"

extern lv_obj_t * ui_MenuPage;
extern int16_t ui_MenuScrollY;
extern Page_t Page_Menu;

void ui_MenuPage_screen_init(void);
void ui_MenuPage_screen_deinit(void);

#ifdef __cplusplus
}
#endif
#endif
