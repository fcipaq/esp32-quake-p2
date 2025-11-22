// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "quakegeneric.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb_hid.h"
#include "hid_keys.h"
#include "quakekeys.h"
#include "control.h"
#include "hwcfg.h"
#include <driver/gpio.h>
#include "bsp/esp-bsp.h"

/*
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
  

*/

//Mapping between USB HID keys and Quake engine keys
const uint8_t key_conv_tab[]={
	[BUTTON_A]=K_ENTER, 
	[BUTTON_B]=K_ESCAPE, 
	[BUTTON_X]=K_BACKSPACE, 
	[BUTTON_Y]=K_SPACE, 
	[BUTTON_L]='a', 
	[BUTTON_R]='b', 
	[BUTTON_SEL]='c', 
	[BUTTON_SW]='d', 
	[BUTTON_ST]=K_TAB, 
	[BUTTON_UP]=K_UPARROW, 
	[BUTTON_DOWN]=K_DOWNARROW, 
	[BUTTON_LEFT]=K_LEFTARROW, 
	[BUTTON_RIGHT]=K_RIGHTARROW, 
/*
        [KEY_TAB]=K_TAB, 
        [KEY_LEFTALT]=K_ALT, 
	[KEY_RIGHTALT]=K_ALT, 
	[KEY_LEFTCTRL]=K_CTRL, 
	[KEY_RIGHTCTRL]=K_CTRL, 
	[KEY_LEFTSHIFT]=K_SHIFT, 
	[KEY_RIGHTSHIFT]=K_SHIFT, 
	[KEY_F1]=K_F1, 
	[KEY_F2]=K_F2, 
	[KEY_F3]=K_F3, 
	[KEY_F4]=K_F4, 
	[KEY_F5]=K_F5, 
	[KEY_F6]=K_F6, 
	[KEY_F7]=K_F7, 
	[KEY_F8]=K_F8, 
	[KEY_F9]=K_F9, 
	[KEY_F10]=K_F10, 
	[KEY_F11]=K_F11, 
	[KEY_F12]=K_F12, 
	[KEY_INSERT]=K_INS, 
	[KEY_DELETE]=K_DEL, 
	[KEY_PAGEDOWN]=K_PGDN, 
	[KEY_PAGEUP]=K_PGUP, 
	[KEY_HOME]=K_HOME, 
	[KEY_END]=K_END,
	[KEY_A]='a',
	[KEY_B]='b',
	[KEY_C]='c',
	[KEY_D]='d',
	[KEY_E]='e',
	[KEY_F]='f',
	[KEY_G]='g',
	[KEY_H]='h',
	[KEY_I]='i',
	[KEY_J]='j',
	[KEY_K]='k',
	[KEY_L]='l',
	[KEY_M]='m',
	[KEY_N]='n',
	[KEY_O]='o',
	[KEY_P]='p',
	[KEY_Q]='q',
	[KEY_R]='r',
	[KEY_S]='s',
	[KEY_T]='t',
	[KEY_U]='u',
	[KEY_V]='v',
	[KEY_W]='w',
	[KEY_X]='x',
	[KEY_Y]='y',
	[KEY_Z]='z',
	[KEY_1]='1',
	[KEY_2]='2',
	[KEY_3]='3',
	[KEY_4]='4',
	[KEY_5]='5',
	[KEY_6]='6',
	[KEY_7]='7',
	[KEY_8]='8',
	[KEY_9]='9',
	[KEY_0]='0',
	[KEY_GRAVE]='`',
	[KEY_DOT]='.',
	[KEY_COMMA]=',',
	[KEY_LEFTBRACE]='[',
	[KEY_RIGHTBRACE]=']',
	[KEY_BACKSLASH]='\\',
	[KEY_SEMICOLON]=';',
	[KEY_APOSTROPHE]='\'',
	[KEY_SLASH]='/',
	[KEY_MINUS]='-',
	[KEY_EQUAL]='=',
	*/
};

static int mouse_dx=0, mouse_dy=0;

void handle_hw() {
    
    if (ctrl_get_btn_state(BUTTON_SEL)) {
        gpio_set_direction(PIN_PWR_EN, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_PWR_EN, 0);
    }
    
    /*
    if (ctrl_get_btn_state(BUTTON_X)) {
        brightness++;
        if (brightness>=100)
        brightness = 0;
    	bsp_display_brightness_set(brightness);
    }
    */

}

int QG_GetKey(int *down, int *key) {
	ctrl_update_btn_state();
	*key=0;
	*down=0;
    
    handle_hw();
    
    for (int i = 0; i < BUTTON_CNT; i++) {
        uint16_t pressed = ctrl_get_btn_press_event(i);
        uint16_t released = ctrl_get_btn_release_event(i);
        if (pressed || released) {
		*down = pressed ? 1 : 0;
		if (i < sizeof(key_conv_tab)/sizeof(key_conv_tab[0])) {
			*key = key_conv_tab[i];
		}
		return 1;
	}
    }
    
    return 0;
}

void QG_GetJoyAxes(float *axes) {
}

void QG_GetMouseMove(int *x, int *y) {
	*x = ctrl_get_ana_y();
	*y = -ctrl_get_ana_x();
}

void input_init() {
   ctrl_init();
   //xTaskCreatePinnedToCore(usb_hid_task, "usbhid", 4096, NULL, 4, NULL, 1);
}


