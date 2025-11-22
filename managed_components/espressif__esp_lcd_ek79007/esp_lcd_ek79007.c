/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lcd_ek79007.h"
#include "../../main/hwcfg.h"

#define EK79007_PAD_CONTROL     (0xB2)
#define EK79007_DSI_2_LANE      (0x10)
#define EK79007_DSI_4_LANE      (0x00)

#define EK79007_CMD_SHLR_BIT    (1ULL << 0)
#define EK79007_CMD_UPDN_BIT    (1ULL << 1)
#define EK79007_MDCTL_VALUE_DEFAULT   (0x01)

#ifndef PICOHELD_REV
#error Pico Held revision is not defined
#endif

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    const ek79007_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    uint8_t lane_num;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} ek79007_panel_t;

static const char *TAG = "ek79007";

static esp_err_t panel_ek79007_send_init_cmds(ek79007_panel_t *ek79007);

static esp_err_t panel_ek79007_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ek79007_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ek79007_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ek79007_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ek79007_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);

esp_err_t esp_lcd_new_panel_ek79007(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_EK79007_VER_MAJOR, ESP_LCD_EK79007_VER_MINOR,
             ESP_LCD_EK79007_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    ek79007_vendor_config_t *vendor_config = (ek79007_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)calloc(1, sizeof(ek79007_panel_t));
    ESP_RETURN_ON_FALSE(ek79007, ESP_ERR_NO_MEM, TAG, "no mem for ek79007 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        ESP_GOTO_ON_ERROR(gpio_reset_pin(panel_dev_config->reset_gpio_num), err, TAG, "failed to setup reset GPIO");
        gpio_set_direction(panel_dev_config->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(panel_dev_config->reset_gpio_num, 0);
    }

    ek79007->io = io;
    ek79007->init_cmds = vendor_config->init_cmds;
    ek79007->init_cmds_size = vendor_config->init_cmds_size;
    ek79007->lane_num = vendor_config->mipi_config.lane_num;
    ek79007->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ek79007->flags.reset_level = panel_dev_config->flags.reset_active_high;
    ek79007->madctl_val = EK79007_MDCTL_VALUE_DEFAULT;

    // Create MIPI DPI panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, ret_panel), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", *ret_panel);

    // Save the original functions of MIPI DPI panel
    ek79007->del = (*ret_panel)->del;
    ek79007->init = (*ret_panel)->init;
    // Overwrite the functions of MIPI DPI panel
    (*ret_panel)->del = panel_ek79007_del;
    (*ret_panel)->init = panel_ek79007_init;
    (*ret_panel)->reset = panel_ek79007_reset;
    (*ret_panel)->mirror = panel_ek79007_mirror;
    (*ret_panel)->invert_color = panel_ek79007_invert_color;
    (*ret_panel)->user_data = ek79007;
    ESP_LOGD(TAG, "new ek79007 panel @%p", ek79007);

    return ESP_OK;

err:
    if (ek79007) {
        free(ek79007);
    }
    return ret;
}

static const ek79007_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
#if PICOHELD_REV == 2
    // Set EXTC
    {0xB9, (uint8_t []) {0xF1, 0x12, 0x83}, 3, 0},
    // Set DSI
    {0xBA, (uint8_t []) {0x31, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, \
                         0x00, 0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0},
    // Set ECP
    {0xB8, (uint8_t []) {0x25, 0x22, 0xF0, 0x63}, 4, 0},
    // Set PCR
    {0xBF, (uint8_t []) {0x02, 0x11, 0x00}, 3, 0},
    // SET RGB
    {0xB3, (uint8_t []) {0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0},
	// Set SCR
    {0xC0, (uint8_t []) {0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00}, 0, 0},
    // Set VDC
    {0xBC, (uint8_t []) {0x46}, 1, 0},
    // Set Panel
    {0xCC, (uint8_t []) {0x0B}, 1, 0},
    // Set Panel Inversion
    {0xB4, (uint8_t []) {0x80}, 1, 0},
    // Set RSO
    {0xB2, (uint8_t []) {0x3C, 0x12, 0x30}, 3, 0},
    // Set EQ
    {0xE3, (uint8_t []) {0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10}, 14, 0},
    // Set POWER	
    {0xC1, (uint8_t []) {0x36, 0x00, 0x32, 0x32, 0x77, 0xF1, 0xCC, 0xCC, 0x77, 0x77, 0x33, 0x33}, 12, 0},
    // Set BGP
    {0xB5, (uint8_t []) {0x0A, 0x0A}, 2, 0},
    // Set VCOM
    {0xB6, (uint8_t []) {0xB2, 0xB2}, 2, 0},
    // Set GIP
    {0xE9, (uint8_t []) {0xC8, 0x10, 0x0A, 0x10, 0x0F, 0xA1, 0x80, 0x12, 0x31, 0x23, 0x47, 0x86, 0xA1, 0x80, 0x47, 0x08,
                         0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x48, 0x02, 0x8B, 0xAF,
			             0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x48, 0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88,
	                     0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 63, 0},
    // Set GIP2
    {0xEA, (uint8_t []) {0x96, 0x12, 0x01, 0x01, 0x01, 0x78, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x31, 0x8B, 0xA8,
                         0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x4F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88,
                         0x88, 0x88, 0x23, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xA1, 0x80, 0x00, 0x00, 0x00, 0x00}, 61, 0},
    // Set Gamma
    {0xE0, (uint8_t []) {0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39, 0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15, 0x10,
	                     0x17, 0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39, 0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15,
	                     0x10, 0x17}, 34, 10},
    // TE on (vsync mode)
    //{0xC7, (uint8_t []) {0x10, 0x00}, 2, 0},

    // Sleep out
    {0x11, (uint8_t []) {0x00}, 0, 50},
    // Display on
    {0x29, (uint8_t []) {0x00}, 0, 50},
#elif PICOHELD_REV == 3
//  {cmd, { data }, data_size, delay_ms}
  {0xB9, (uint8_t []) {0xF1, 0x12, 0x87}, 3, 0},

  {0xB2, (uint8_t []) {0xB4, 0x03, 0X70}, 3, 0},

  {0xB3, (uint8_t []) {0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0},

  {0xB4, (uint8_t []) {0x80}, 1, 0},

  {0xB5, (uint8_t []) {0x0A, 0x0A}, 2, 0},

  {0xB6, (uint8_t []) {0x8D, 0x8D}, 2, 0},

  {0xB8, (uint8_t []) {0x26, 0x22, 0xF0, 0x13}, 4, 0},

  {0xBA, (uint8_t []) {0x31, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x91, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0},

  {0xBC, (uint8_t []) {0x47}, 1, 0},

  {0xBF, (uint8_t []) {0x02, 0x10, 0x00, 0x80, 0x04}, 5, 0},

  {0xC0, (uint8_t []) {0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x73, 0x00}, 9, 0},

  {0xC1, (uint8_t []) {0x36, 0x00, 0x32, 0x32, 0x77, 0xE1, 0x77, 0x77, 0xCC, 0xCC, 0xFF, 0xFF, 0x11, 0x11, 0x00, 0x00, 0x32}, 17, 0},

  {0xC7, (uint8_t []) {0x10, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0xED, 0xC5, 0x00, 0xA5}, 12, 0},

  {0xC8, (uint8_t []) {0x10, 0x40, 0x1E, 0x03}, 4, 0},

  {0xCC, (uint8_t []) {0x0B}, 1, 0},

  {0xE0, (uint8_t []) {0x00, 0x0A, 0x0F, 0x2A, 0x33, 0x3F, 0x44, 0x39, 0x06, 0x0C, 0x0E, 0x14, 0x15, 0x13, 0x15, 0x10, 0x18, 0x00, 0x0A, 0x0F, 0x2A, 0x33, 0x3F, 0x44, 0x39, 0x06, 0x0C, 0x0E, 0x14, 0x15, 0x13, 0x15, 0x10, 0x18}, 34, 0},

  {0xE1, (uint8_t []) {0x11, 0x11, 0x91, 0x00, 0x00, 0x00, 0x00}, 7, 0},

  {0xE3, (uint8_t []) {0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x04, 0xC0, 0x10}, 14, 0},

  {0xE9, (uint8_t []) {0xC8, 0x10, 0x0A, 0x00, 0x00, 0x80, 0x81, 0x12, 0x31, 0x23, 0x4F, 0x86, 0xA0, 0x00, 0x47, 0x08, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x98, 0x02, 0x8B, 0xAF, 0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x98, 0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 63, 0},

  {0xEA, (uint8_t []) {0x97, 0x0C, 0x09, 0x09, 0x09, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x31, 0x8B, 0xA8, 0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x9F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88, 0x88, 0x88, 0x23, 0x00, 0x00, 0x02, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x81, 0x00, 0x00, 0x00, 0x00}, 61, 0},

  {0xEF, (uint8_t []) {0xFF, 0xFF, 0x01}, 3, 0},

  // TE on (vsync mode)
  //{0xC7, (uint8_t []) {0x10, 0x00}, 2, 0},

  {0x11, (uint8_t []) {0x00}, 0, 250},

  {0x29, (uint8_t []) {0x00}, 0, 50},
#endif
};

static esp_err_t panel_ek79007_send_init_cmds(ek79007_panel_t *ek79007)
{
    esp_lcd_panel_io_handle_t io = ek79007->io;
    const ek79007_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    uint8_t lane_command = EK79007_DSI_2_LANE;
    bool is_cmd_overwritten = false;

    switch (ek79007->lane_num) {
    case 0:
    case 2:
        lane_command = EK79007_DSI_2_LANE;
        break;
    case 4:
        lane_command = EK79007_DSI_4_LANE;
        break;
    default:
        ESP_LOGE(TAG, "Invalid lane number %d", ek79007->lane_num);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, EK79007_PAD_CONTROL, (uint8_t[]) {
        lane_command,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (ek79007->init_cmds) {
        init_cmds = ek79007->init_cmds;
        init_cmds_size = ek79007->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(ek79007_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                ek79007->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                is_cmd_overwritten = false;
                break;
            }

            if (is_cmd_overwritten) {
                is_cmd_overwritten = false;
                ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                         init_cmds[i].cmd);
            }
        }

        // Send command
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }

    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_ek79007_del(esp_lcd_panel_t *panel)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;

    // Delete MIPI DPI panel
    if (ek79007->reset_gpio_num >= 0) {
        gpio_reset_pin(ek79007->reset_gpio_num);
        gpio_set_direction(ek79007->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(ek79007->reset_gpio_num, 0);
    }
    ESP_LOGD(TAG, "del ek79007 panel @%p", ek79007);
    free(ek79007);

    return ESP_OK;
}

static esp_err_t panel_ek79007_init(esp_lcd_panel_t *panel)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;

    ESP_RETURN_ON_ERROR(panel_ek79007_send_init_cmds(ek79007), TAG, "send init commands failed");
    ESP_RETURN_ON_ERROR(ek79007->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_ek79007_reset(esp_lcd_panel_t *panel)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = ek79007->io;

    // Perform hardware reset
    if (ek79007->reset_gpio_num >= 0) {
#if PICOHELD_REV == 2
        gpio_reset_pin(ek79007->reset_gpio_num);
        gpio_set_direction(ek79007->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(ek79007->reset_gpio_num, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ek79007->reset_gpio_num, 1);
        vTaskDelay(pdMS_TO_TICKS(150));
#elif PICOHELD_REV == 3
        gpio_reset_pin(ek79007->reset_gpio_num);
        gpio_set_direction(ek79007->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(ek79007->reset_gpio_num, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_direction(ek79007->reset_gpio_num, GPIO_MODE_INPUT);
        // floating means 1, since external pullup is connected
        // !! Do not set to 1, since IOVCC is 1.8V !!
        gpio_set_pull_mode(ek79007->reset_gpio_num, GPIO_FLOATING);
        vTaskDelay(pdMS_TO_TICKS(150));
#endif
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_ek79007_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = ek79007->io;
    uint8_t madctl_val = ek79007->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // Control mirror through LCD command
    if (mirror_x) {
        madctl_val |= EK79007_CMD_SHLR_BIT;
    } else {
        madctl_val &= ~EK79007_CMD_SHLR_BIT;
    }
    if (mirror_y) {
        madctl_val |= EK79007_CMD_UPDN_BIT;
    } else {
        madctl_val &= ~EK79007_CMD_UPDN_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        madctl_val
    }, 1), TAG, "send command failed");
    ek79007->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_ek79007_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = ek79007->io;
    uint8_t command = 0;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}
#endif
