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

static QueueHandle_t s_screen_manager_queue;
static SSD1306_t s_device;


typedef struct {
    char command;
    char message[64];
} ScreenCommand_t;

// debug to screen function
// takes process string, err-code, and screen queue handle, generates screencommand for the commands and sends it to screen queue to be printet
void debug_to_screen_queue(char *process_to_debug_name, esp_err_t error, QueueHandle_t queue)
{
    ScreenCommand_t err_message_to_screen = {
        .command = 'p',
    };

    if (error == ESP_OK) {
        snprintf(err_message_to_screen.message, 64 - 1, "%-11s [ok]", process_to_debug_name);
        xQueueSend(queue, &err_message_to_screen, 0);
    } else {
        snprintf(err_message_to_screen.message, 64 - 1, "%s: bad", process_to_debug_name);
        xQueueSend(queue, &err_message_to_screen, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        strcpy(err_message_to_screen.message, esp_err_to_name(error));
        xQueueSend(queue, &err_message_to_screen, 0);
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
    }
}


void init_wifi(void *args)
{
    esp_err_t err;

    err = nvs_flash_init(); // this is neeed to init wifi
    debug_to_screen_queue("nvs flash", err, s_screen_manager_queue);
    ;

    wifi_init_config_t w_init_config = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t st_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        }
    };

    err = esp_wifi_init(&w_init_config);
    debug_to_screen_queue("w-init", err, s_screen_manager_queue);

    esp_wifi_start();
    debug_to_screen_queue("w-start", err, s_screen_manager_queue);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    debug_to_screen_queue("w-mode", err, s_screen_manager_queue);

    err = esp_wifi_set_config(WIFI_IF_STA, &st_config);
    debug_to_screen_queue("w-conf", err, s_screen_manager_queue);

    err = esp_wifi_connect();
    vTaskDelay(pdMS_TO_TICKS(4000));
    debug_to_screen_queue("w-conn", err, s_screen_manager_queue);

    wifi_ap_record_t ap_record;
    esp_wifi_sta_get_ap_info(&ap_record);
    ScreenCommand_t wifi_name_command = {
        .command = 'p'
    };
    strcpy(wifi_name_command.message, (char *) ap_record.ssid);
    xQueueSend(s_screen_manager_queue, &wifi_name_command, 0);
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
    int page_counter = 0;

    while (1) {
        if (xQueueReceive(s_screen_manager_queue, &recived_message, 0) == pdTRUE) {
            switch (recived_message.command) { // screen manager command palette
            case 'p': // print terminal style (mostly for debug)
                if (page_counter > 8) {
                    page_counter = 0;
                    ssd1306_clear_screen(&s_device, false);
                }
                if (strlen(recived_message.message) <= 16) {
                    ssd1306_display_text(&s_device, page_counter, recived_message.message, strlen(recived_message.message), false);
                } else {
                    ssd1306_display_text_box1(&s_device, page_counter, 0, recived_message.message, 16, strlen(recived_message.message), false, 50);
                }
                page_counter++;
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    s_screen_manager_queue = xQueueCreate(10, sizeof(ScreenCommand_t));
    xTaskCreate(screen_manager, "init_wifi", 4096, NULL, 11, NULL);
    xTaskCreate(init_wifi, "init_wifi", 4096, NULL, 10, NULL);

    // ssd1306_bitmaps(&device, 0, 0, image_1, 128, 64, false);

}