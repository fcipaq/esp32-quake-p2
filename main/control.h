#pragma once

#include <stdint.h>
#include <stdbool.h>

enum {
  BUTTON_A    = 0,
  BUTTON_B    = 1,
  BUTTON_X    = 2,
  BUTTON_Y    = 3,
  BUTTON_LEFT = 4,
  BUTTON_RIGHT = 5,
  BUTTON_UP   = 6,
  BUTTON_DOWN = 7,
  BUTTON_ST   = 8,
  BUTTON_SEL  = 9,
  BUTTON_L    = 10,
  BUTTON_R    = 11,
  BUTTON_SW   = 12,
  BUTTON_CNT  // last item
};

void ctrl_init();
uint32_t ctrl_update_btn_state();
bool ctrl_get_btn_state(uint32_t button);
bool ctrl_get_btn_press_event(uint32_t button);
bool ctrl_get_btn_release_event(uint32_t button);
int ctrl_get_ana_x();
int ctrl_get_ana_y();
