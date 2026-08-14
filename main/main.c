#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include "bitmaps/number_bitmaps.c"

/*
 You have to set this config value with menuconfig
 CONFIG_INTERFACE

 for SPI
 CONFIG_CS_GPIO
 CONFIG_DC_GPIO
 CONFIG_RESET_GPIO
*/

#define tag "SSD1306"

static QueueHandle_t queue;
static SSD1306_t device;

void send_queue(void *arg){
	while(1){
		for (int num = 10; num > 0; num--) {
			xQueueSend(queue, (void *)&num, pdMS_TO_TICKS(0));
			vTaskDelay(pdMS_TO_TICKS(500));
		}
	}
}

void recieve_queue(void *arg){
	int a;
	
	while(1){
		if (xQueueReceive(queue, (void *)&a, pdMS_TO_TICKS(100))) { // den skal have en adresse den kan skrive i
			// ssd1306_clear_screen(&device, false);
			ESP_LOGI("switch", "number recieved: %d", a);
		}
	}
}

void app_main(void)
{
	spi_master_init(&device, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
	ssd1306_init(&device, 128, 64);
	ssd1306_clear_screen(&device, false);
	ssd1306_contrast(&device, 0xFF);
	ssd1306_bitmaps(&device, 0, 0, image_2, 64, 64, false);

	queue = xQueueCreate(5, sizeof(int));
	xTaskCreate(send_queue, "send_queue", 4096, NULL, 10, NULL);
	xTaskCreate(recieve_queue, "recieve_queue", 4096, NULL, 10, NULL);
}