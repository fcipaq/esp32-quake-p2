#pragma once

#define PICOHELD_REV (3)

#define PIN_LCD_BL_EN (GPIO_NUM_7)
#define PIN_LCD_BL_PWM (GPIO_NUM_22)

#if PICOHELD_REV==2
#define PIN_PWR_EN (GPIO_NUM_53)
#define PIN_PWR_CHG_STAT (GPIO_NUM_52)
#elif PICOHELD_REV==3
#define PIN_PWR_EN (GPIO_NUM_52)
#define PIN_PWR_CHG_STAT (GPIO_NUM_53)
#elif
#error unsupported Pico Held revision
#endif

#define PIN_SND_EN (GPIO_NUM_30)
#define PIN_I2S_BCK (28)
#define PIN_I2S_WS (29)
#define PIN_I2S_DOUT (25)

#define PIN_SD_DAT0_MISO (GPIO_NUM_39)
#define PIN_SD_DAT1 (GPIO_NUM_40)
#define PIN_SD_DAT2 (GPIO_NUM_41)
#define PIN_SD_DAT3_CS (GPIO_NUM_42)
#define PIN_SD_SCK (GPIO_NUM_43)
#define PIN_SD_CMD_MOSI (GPIO_NUM_44)
#define PIN_SD_DET (GPIO_NUM_47)

#define PIN_BTN_A (GPIO_NUM_31)
#define PIN_BTN_B (GPIO_NUM_5)
#define PIN_BTN_X (GPIO_NUM_32)
#define PIN_BTN_Y (GPIO_NUM_54)
#define PIN_BTN_LEFT (GPIO_NUM_14)
#define PIN_BTN_RGHT (GPIO_NUM_12)

#define PIN_BTN_UP (GPIO_NUM_13)
#define PIN_BTN_DOWN (GPIO_NUM_15)
#define PIN_BTN_ST (GPIO_NUM_45)
#define PIN_BTN_SEL (GPIO_NUM_35)
#define PIN_BTN_L (GPIO_NUM_11)
#define PIN_BTN_R (GPIO_NUM_4)
#define PIN_BTN_SW (GPIO_NUM_1)

#define PIN_ANA_X (GPIO_NUM_16)
#define PIN_ANA_Y (GPIO_NUM_17)

#define PIN_STAT_LED (GPIO_NUM_33)

#define BUTTON_PRESSED_WHEN_LO (0)
#define BUTTON_PRESSED_WHEN_HI (1)
