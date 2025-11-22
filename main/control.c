#include "control.h"
#include "hwcfg.h"
#include <driver/gpio.h>
#include "driver/adc.h"
#include "timing.h"

typedef struct
{
  int              pin_id;
  gpio_num_t       gpio_num;
  int              key_state;
  int              key_newly_pressed;
  int              key_newly_released;
  gpio_pull_mode_t pull_mode;
  int              pressed_state;
  uint32_t         key_timer;
} keymap_t;

/*
keymap_t keymap[BUTTON_CNT] = {
  [BUTTON_A] = { BUTTON_A,     PIN_BTN_A,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 }, 
  { BUTTON_B,     PIN_BTN_B,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_X,     PIN_BTN_X,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },      
  { BUTTON_Y,     PIN_BTN_Y,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_LEFT,  PIN_BTN_LEFT,  0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_RIGHT, PIN_BTN_RGHT,  0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_UP,    PIN_BTN_UP,    0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_DOWN,  PIN_BTN_DOWN,  0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_ST,    PIN_BTN_ST,    0, GPIO_PULLDOWN_ONLY, BUTTON_PRESSED_WHEN_HI, 0 },
  { BUTTON_SEL,   PIN_BTN_SEL,   0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_L,     PIN_BTN_L,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_R,     PIN_BTN_R,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  { BUTTON_SW,    PIN_BTN_SW,    0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 }
};
*/

keymap_t keymap[BUTTON_CNT] = {
  [BUTTON_A]     = { BUTTON_A,     PIN_BTN_A,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 }, 
  [BUTTON_B]     = { BUTTON_B,     PIN_BTN_B,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_X]     = { BUTTON_X,     PIN_BTN_X,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },      
  [BUTTON_Y]     = { BUTTON_Y,     PIN_BTN_Y,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_LEFT]  = { BUTTON_LEFT,  PIN_BTN_LEFT,  0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_RIGHT] = { BUTTON_RIGHT, PIN_BTN_RGHT,  0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_UP]    = { BUTTON_UP,    PIN_BTN_UP,    0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_DOWN]  = { BUTTON_DOWN,  PIN_BTN_DOWN,  0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_ST]    = { BUTTON_ST,    PIN_BTN_ST,    0, GPIO_PULLDOWN_ONLY, BUTTON_PRESSED_WHEN_HI, 0 },
  [BUTTON_SEL]   = { BUTTON_SEL,   PIN_BTN_SEL,   0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_L]     = { BUTTON_L,     PIN_BTN_L,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_R]     = { BUTTON_R,     PIN_BTN_R,     0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 },
  [BUTTON_SW]    = { BUTTON_SW,    PIN_BTN_SW,    0, GPIO_PULLUP_ONLY,   BUTTON_PRESSED_WHEN_LO, 0 }
};

typedef struct {
  int min;
  int center;
  int max;
} ana_cal_t;

ana_cal_t ana_cal_x = {
  .min = 1000,
  .max = 2000
};

ana_cal_t ana_cal_y = {
  .min = 1000,
  .max = 2000
};

void ctrl_ana_calib() {
   int num_samples = 20;
   
   int cal_val = 0;
   int start_over = 0;
   int tolerance = 10; // in percent

   do {
     start_over = 0;
     for (int i = 0; i < num_samples; i++) {
        int adc_reading = adc1_get_raw((adc1_channel_t) 0);
        cal_val = (cal_val * i + adc_reading) / (i + 1);
        if ((adc_reading * 100 > cal_val * (100 + tolerance)) || (adc_reading * 100 < cal_val * (100 - tolerance))) {
          start_over = 1;
          break;
        }
     }
   } while (start_over);

   ana_cal_x.center = cal_val;
   
   do {
     start_over = 0;
     for (int i = 0; i < num_samples; i++) {
        int adc_reading = adc1_get_raw((adc1_channel_t) 1);
        cal_val = (cal_val * i + adc_reading) / (i + 1);
        if ((adc_reading * 100 > cal_val * (100 + tolerance)) || (adc_reading * 100 < cal_val * (90 - tolerance))) {
          start_over = 1;
          break;
        }
     }
   } while (start_over);

   ana_cal_y.center = cal_val;
   
   printf("Analog center position is: x: %d, y: %d\n", ana_cal_x.center, ana_cal_y.center);
}

int ctrl_get_ana_x() {
  
    int num_samples = 10;
    int adc_reading = 0;
  
    for (int i = 0; i < num_samples; i++) {
      adc_reading += adc1_get_raw((adc1_channel_t) 0);
    }
    
    adc_reading /= num_samples;
    
    int ret;
    
    if (adc_reading < ana_cal_x.min)
      ana_cal_x.min = adc_reading;

    if (adc_reading > ana_cal_x.max)
      ana_cal_x.max = adc_reading;

    if (adc_reading > ana_cal_x.center) {
      ret = (adc_reading - ana_cal_x.center) * 100 / (ana_cal_x.max - ana_cal_x.center);
      if (ret > 100)
        ret = 100;
      if (ret < 18)  // reject
        ret = 0;
    } else {
      ret = (adc_reading - ana_cal_x.center) * 100 / (ana_cal_x.min - ana_cal_x.center);
      if (ret > 100)
        ret = 100;
      if (ret < 18)  // reject
        ret = 0;
      ret = -ret;
    }

    return ret;
}

int ctrl_get_ana_y() {
  
    int num_samples = 10;
    int adc_reading = 0;
  
    for (int i = 0; i < num_samples; i++) {
      adc_reading += adc1_get_raw((adc1_channel_t) 1);
    }
    
    adc_reading /= num_samples;

    if (adc_reading < ana_cal_y.min)
      ana_cal_y.min = adc_reading;

    if (adc_reading > ana_cal_y.max)
      ana_cal_y.max = adc_reading;

    int ret;
    
    if (adc_reading > ana_cal_y.center) {
      ret = (adc_reading - ana_cal_y.center) * 100 / (ana_cal_y.max - ana_cal_y.center);
      if (ret > 100)
        ret = 100;
      if (ret < 18)  // reject
        ret = 0;
    } else {
      ret = (adc_reading - ana_cal_y.center) * 100 / (ana_cal_y.min - ana_cal_y.center);
      if (ret > 100)
        ret = 100;
      if (ret < 18)  // reject
        ret = 0;
      ret = -ret;
    }

    return ret;
}

void ctrl_init() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(0, ADC_ATTEN_DB_0);  // x
  adc1_config_channel_atten(1, ADC_ATTEN_DB_0);  // y
        
  ctrl_ana_calib();
        
  for (int i = 0; i < BUTTON_CNT; i++) {
    gpio_set_direction(keymap[i].gpio_num, GPIO_MODE_INPUT);
    gpio_set_pull_mode(keymap[i].gpio_num, keymap[i].pull_mode);
  }
}

uint32_t ctrl_update_btn_state() {
  uint32_t res = 0;
  
  for (int i = 0; i < BUTTON_CNT; i++) {
    if (gpio_get_level(keymap[i].gpio_num) == keymap[i].pressed_state) {
      if (keymap[i].key_state == 0) {
        keymap[i].key_state = 1;
        keymap[i].key_newly_released = 0;
        keymap[i].key_newly_pressed = 1;
        keymap[i].key_timer = millis();
      }
      res |= 1u << i;
    } else {
      if (keymap[i].key_state == 1) { 
        keymap[i].key_newly_pressed = 0;
        keymap[i].key_newly_released = 1;
        keymap[i].key_state = 0;
      }
    }
  }
  
  return res;
}

bool ctrl_get_btn_state(uint32_t button) {
  if (button > BUTTON_CNT)
    return false;

  if (keymap[button].key_state)
    return true;
  else
    return false;
}

bool ctrl_get_btn_press_event(uint32_t button) {
  if (button > BUTTON_CNT)
    return false;

  uint16_t key_newly_pressed = keymap[button].key_newly_pressed;
  
  keymap[button].key_newly_pressed = 0; 

  return key_newly_pressed;
}

bool ctrl_get_btn_release_event(uint32_t button) {
  if (button > BUTTON_CNT)
    return false;

  uint16_t key_newly_released = keymap[button].key_newly_released;
  
  keymap[button].key_newly_released = 0;

  return key_newly_released;
}
