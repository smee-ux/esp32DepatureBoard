#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

//gpio and oled screen stuff
#include "ssd1306.h"
#include "bitmaps/train_bitmap.c"

#include "esp_wifi.h"
#include "nvs_flash.h"

//event groups
#include "freertos/event_groups.h"




//wifi stuff
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

#define tag "SSD1306"

static QueueHandle_t s_screenManagerQueue;
static SSD1306_t s_device;


typedef struct {
    char command;
    char message[64];
} ScreenCommand_t;


void init_wifi(void *args)
{
    esp_err_t err;
    ScreenCommand_t err_message_to_screen;

    err = nvs_flash_init(); // this is neeed to init wifi
    if (err == ESP_OK) {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "nvs_flash: ok");
    } else {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "nvs_flash: bad");
        vTaskDelay(pdMS_TO_TICKS(1000));
        strcpy(err_message_to_screen.message, esp_err_to_name(err));
    }
    xQueueSend(s_screenManagerQueue, &err_message_to_screen, 0);

    wifi_init_config_t w_init_config = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t st_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        }
    };

    err = esp_wifi_init(&w_init_config);


    ESP_ERROR_CHECK(esp_wifi_start());
    if (err == ESP_OK) {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-start: ok");
    } else {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-start: bad");
        vTaskDelay(pdMS_TO_TICKS(1000));
        strcpy(err_message_to_screen.message, esp_err_to_name(err));
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-mode: ok");
    } else {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-mode: bad");
        vTaskDelay(pdMS_TO_TICKS(1000));
        strcpy(err_message_to_screen.message, esp_err_to_name(err));
    }
    xQueueSend(s_screenManagerQueue, &err_message_to_screen, 0);

    err = esp_wifi_set_config(WIFI_IF_STA, &st_config);
    if (err == ESP_OK) {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-conf: ok");
    } else {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-conf: bad");
        vTaskDelay(pdMS_TO_TICKS(1000));
        strcpy(err_message_to_screen.message, esp_err_to_name(err));
    }
    xQueueSend(s_screenManagerQueue, &err_message_to_screen, 0);

    err = esp_wifi_connect();
    vTaskDelay(pdMS_TO_TICKS(4000));
    if (err == ESP_OK) {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-conn: ok");
    } else {
        err_message_to_screen.command = 'p';
        strcpy(err_message_to_screen.message, "w-conn: bad");
        vTaskDelay(pdMS_TO_TICKS(1000));
        strcpy(err_message_to_screen.message, esp_err_to_name(err));
    }
    xQueueSend(s_screenManagerQueue, &err_message_to_screen, 0);

    wifi_ap_record_t ap_record;
    esp_wifi_sta_get_ap_info(&ap_record);
    ScreenCommand_t wifi_name_command = {
        .command = 'p'
    };
    strcpy(wifi_name_command.message, (char *) ap_record.ssid);
    xQueueSend(s_screenManagerQueue, &wifi_name_command, 0);
    vTaskDelete(NULL);
}


void screen_manager(void *args)
{
    // Initialize
    spi_master_init(&s_device, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&s_device, 128, 64);
    ssd1306_clear_screen(&s_device, false);
    ssd1306_contrast(&s_device, 0xFF);


    ScreenCommand_t recived_message;
    int page_counter = 1;

    while (1) {
        if (xQueueReceive(s_screenManagerQueue, &recived_message, 0) == pdTRUE) {
            switch (recived_message.command) { // screen manager command palette
            case 'p': // print terminal style
                if (page_counter > 8) {
                    page_counter = 0;
                    ssd1306_clear_screen(&s_device, false);
                }
                ssd1306_display_text(&s_device, page_counter, recived_message.message, strlen(recived_message.message), false);
                page_counter++;
                break;

            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    s_screenManagerQueue = xQueueCreate(10, sizeof(ScreenCommand_t));
    xTaskCreate(screen_manager, "init_wifi", 4096, NULL, 11, NULL);
    xTaskCreate(init_wifi, "init_wifi", 4096, NULL, 10, NULL);
    // send_http_request();
    // test
    // ssd1306_bitmaps(&device, 0, 0, image_1, 128, 64, false);

    // queue = xQueueCreate(5, sizeof(int));
}