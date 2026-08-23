#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
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

static QueueHandle_t screenManagerQueue;

static SSD1306_t device;


struct ScreenCommand {
	char command;
	char message[64];
} ScreenCommand;


void init_wifi(void *args){
	nvs_flash_init(); // this is neeed to init wifi
	wifi_init_config_t cola = WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t st_config = {
		.sta = {
			.ssid = ESP_WIFI_SSID,
			.password = ESP_WIFI_PASS,
		}
	};

	esp_err_t err;
	struct ScreenCommand err_message_to_screen;
	err = esp_wifi_init(&cola);
	// ESP_LOGI("wifi init", "%s", esp_err_to_name(err));

	ESP_ERROR_CHECK(esp_wifi_start());
	// ESP_LOGI("wifi start", "%s", esp_err_to_name(err));

	err = esp_wifi_set_mode(WIFI_MODE_STA);
	if (err == ESP_OK) {
		err_message_to_screen.command = 'p';
		strcpy(err_message_to_screen.message, "w-mode: ok");
	}
	else {
		err_message_to_screen.command = 'p';
		strcpy(err_message_to_screen.message, "w-mode: bad");
		xQueueSend(screenManagerQueue, &err_message_to_screen, 0);
		vTaskDelay(pdMS_TO_TICKS(1000));
		strcpy(err_message_to_screen.message, esp_err_to_name(err));
	}
	xQueueSend(screenManagerQueue, &err_message_to_screen, 0);

	err = esp_wifi_set_config(WIFI_IF_STA, &st_config);
	// ESP_LOGI("wifi config", "%s", esp_err_to_name(err));

	err = esp_wifi_connect();
	vTaskDelay(pdMS_TO_TICKS(4000));
	// ESP_LOGI("wifi connect", "%s", esp_err_to_name(err));

	wifi_ap_record_t ap_record;
	esp_wifi_sta_get_ap_info(&ap_record);

	ESP_LOGI("rap_record", "%s", ap_record.ssid);

	vTaskDelete(NULL);
}


void screen_manager(void *args){
	// Initialize
	spi_master_init(&device, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
	ssd1306_init(&device, 128, 64);
	ssd1306_clear_screen(&device, false);
	ssd1306_contrast(&device, 0xFF);

	screenManagerQueue = xQueueCreate(10, sizeof(ScreenCommand));
	struct ScreenCommand recived_message;

	while (1) {
		if (xQueueReceive(screenManagerQueue, &recived_message, 0) == pdTRUE){
			switch (recived_message.command) { // screen manager command palette
				case 'p': // print command
					ssd1306_clear_screen(&device, false);
					ssd1306_display_text(&device, 3, recived_message.message, strlen(recived_message.message), false);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void app_main(void)
{
	xTaskCreate(screen_manager, "init_wifi", 4096, NULL, 10, NULL);
	xTaskCreate(init_wifi, "init_wifi", 4096, NULL, 10, NULL);
	// send_http_request();
	// test
	// ssd1306_bitmaps(&device, 0, 0, image_1, 128, 64, false);

	// queue = xQueueCreate(5, sizeof(int));
	// xTaskCreate(send_queue, "send_queue", 4096, NULL, 10, NULL);
	// xTaskCreate(recieve_queue, "recieve_queue", 4096, NULL, 10, NULL);
}