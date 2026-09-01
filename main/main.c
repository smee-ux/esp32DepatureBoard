#include "bitmaps/train_bitmap.c"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_wifi_default.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "portmacro.h"
#include "ssd1306.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>



//  wifi stuff
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY   5
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

//  screen stuff
static QueueHandle_t s_screen_manager_queue;
static SSD1306_t s_device;

// http stuff
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define HTTP_GET_URL enter_url_here

typedef struct {
    char command;
    char message[64];
} ScreenCommand_t;

// debug to screen function
// takes process string, err-code, and screen queue handle, generates screencommand for the commands and sends it to screen queue to be printet
void print_debug_to_screen_queue(char *process_to_debug_name, esp_err_t error, QueueHandle_t queue)
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

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("wifi con", "retry to connect to the ");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        } 
        ESP_LOGI("wifi fail","connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t *) event_data; // get ip event has data containing the ip
        ESP_LOGI("ip:", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void init_wifi()
{
    s_wifi_event_group = xEventGroupCreate();

    esp_err_t err;

    esp_netif_create_default_wifi_sta(); // abstraction layer between drivers and tcp/ip stack,  allows for some nice event handling

    err = esp_netif_init(); // skal kaldes for at start tcp ip stackd
    print_debug_to_screen_queue("netif", err, s_screen_manager_queue);
    
  
    wifi_init_config_t w_init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&w_init_config);
    print_debug_to_screen_queue("w-init", err, s_screen_manager_queue);
    
    wifi_config_t st_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        }
    };

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    err = esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id //output parameter
                                                        );
    print_debug_to_screen_queue("WeventHan", err, s_screen_manager_queue);
    err = esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip);
    print_debug_to_screen_queue("IeventHan", err, s_screen_manager_queue);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    print_debug_to_screen_queue("w-mode", err, s_screen_manager_queue);


  
    err = esp_wifi_set_config(WIFI_IF_STA, &st_config);
    print_debug_to_screen_queue("w-conf", err, s_screen_manager_queue);


    err = esp_wifi_start();
    print_debug_to_screen_queue("w-start", err, s_screen_manager_queue);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("wifi connect", "connected to ap SSID:%s", ESP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI("wifi connect", "failed to connect to SSID: %s", ESP_WIFI_PASS);
    } else {
        ESP_LOGE("wifi connect", "yo wtf just happened");
    }

}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char* TAG = "http event hander";
    static char *output_buffer; // stores response of http request
    static int output_len; // stores number of bytes red
    switch(evt->event_id) {// samme som (*evt).event_id
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_HEADERS_COMPLETE:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADERS_COMPLETE");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA len=%d", evt->data_len);
            // buffer cleaning if its a new request?
            if (output_len == 0 && evt->user_data) {
                // user data is OUR LOCAL BUFFER which data gets sent to
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            // check if response header says if reponse if chunked, if not do:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = 0;
                if (evt->user_data) { // hvis der er defineret et sted til user_data
                    // last byte in evt-user_data is saved for null char 
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    } 
                } else { // her bliver der manuelt dynamisk allokeret hukkomelse til svaret med calloc
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");        
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }        
            output_len=0;
            break;
        // herfra no clue hvad der sker noget med noget debugging spørg
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void http_get(void){
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    
    esp_http_client_config_t config = {
        .url = HTTP_GET_URL,
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
        .disable_auto_redirect = true,
    };
    ESP_LOGI("http get", "HTTP request with url =>");
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI("http get", "HTTP GET Status = %d, content_length= %"PRId64,   //PRId64 expander til den system relevante format specifier for en 64bit singed integer
                                                                                    // grunden til det er " og så MACRO'en er fordi macroen expander med quotation marks om sig og så er resten concatenation fx "...%" "lld" = %lld
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                ssd1306_display_text_box1(&s_device, 3, 48, local_response_buffer, 10, 10, false, 0);
    } else {
        ESP_LOGE("http get", "HTTP GET request failed: %s", esp_err_to_name(err));
    }    
}


void app_main(void)
{
    xTaskCreate(screen_manager, "init_wifi", 4096, NULL, 11, NULL);
    s_screen_manager_queue = xQueueCreate(10, sizeof(ScreenCommand_t));
    esp_event_loop_create_default();
      // initializing nvs
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    print_debug_to_screen_queue("nvs flash", err, s_screen_manager_queue);

    init_wifi();

    
    
    ssd1306_clear_screen(&s_device, false);
    ssd1306_bitmaps(&s_device, 0, 0, image_1, 128, 64, false);
    http_get();

}