#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

//gpio and oled screen stuff
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include "bitmaps/train_bitmap.c"

#include "esp_wifi.h"
#include "nvs_flash.h"

//wifi stuff
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

#define tag "SSD1306"

static QueueHandle_t queue;
static SSD1306_t device;


void send_queue(void *arg){
	while(1){
		for (int num = 10; num > 0; num--) {
			xQueueSend(queue, (void *)&num, pdMS_TO_TICKS(0));
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
	}
}

void recieve_queue(void *arg){
	int a;
	char* line = "C ";
	
	while(1){
		if (xQueueReceive(queue, (void *)&a, pdMS_TO_TICKS(100))) { // den skal have en adresse den kan skrive i
			ESP_LOGI("switch", "number recieved: %d", a);
			
			// convert recieved number to a string that can be passed to ssd1306
			char departure_string[10];
			snprintf(departure_string, 10, "%s:%d min", line,a);			
			ssd1306_display_text_box1(&device, 3, 48, departure_string, 10, 10, false, 0);
		}	
	}
}

void init_wifi(){
	nvs_flash_init(); // this is neeed to init wifi
	wifi_init_config_t this = WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t st_config = {
		.sta = {
			.ssid = ESP_WIFI_SSID,
			.password = ESP_WIFI_PASS,
		}
	};

	esp_err_t err;

	err = esp_wifi_init(&this);
	ESP_LOGI("wifi init", "%s", esp_err_to_name(err));

	err = esp_wifi_start();
	ESP_LOGI("wifi start", "%s", esp_err_to_name(err));

	err = esp_wifi_set_mode(WIFI_MODE_STA);
	ESP_LOGI("wifi set mode", "%s", esp_err_to_name(err));

	err = esp_wifi_set_config(WIFI_IF_STA, &st_config);
	ESP_LOGI("wifi connect", "%s", esp_err_to_name(err));

	esp_wifi_scan_start(NULL, true);
	wifi_ap_record_t wifArr[13];
	uint16_t length = 12;
	esp_wifi_scan_get_ap_records(&length, wifArr);
	for(int i = 0; i < length; i++){
		ESP_LOGI("scan", "ap %d, %s", i, wifArr[i].ssid);
	}

	err = esp_wifi_connect();
	ESP_LOGI("wifi connect", "%s", esp_err_to_name(err));
	
	

}

void app_main(void)
{

	init_wifi();
	//
	// spi_master_init(&device, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
	// ssd1306_init(&device, 128, 64);
	// ssd1306_clear_screen(&device, false);
	// ssd1306_contrast(&device, 0xFF);
	// ssd1306_bitmaps(&device, 0, 0, image_1, 128, 64, false);

	// queue = xQueueCreate(5, sizeof(int));
	// xTaskCreate(send_queue, "send_queue", 4096, NULL, 10, NULL);
	// xTaskCreate(recieve_queue, "recieve_queue", 4096, NULL, 10, NULL);
}