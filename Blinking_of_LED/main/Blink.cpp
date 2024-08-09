#include <iostream>
#include <bits/stdc++.h>
#include <cstdint>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define LED_GPIO GPIO_NUM_2
static const char *TAG = "LED_BLINK";

extern "C" void app_main(void)
{
    std::cout << "Hello world! by c++" << std::endl;
    ESP_LOGI(TAG, "Starting LED Blink Application");
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (true) {

        gpio_set_level(LED_GPIO, 1); 
        ESP_LOGI(TAG, "LED turned ON");
        vTaskDelay(1000 / portTICK_PERIOD_MS); 

        gpio_set_level(LED_GPIO, 0);  
        ESP_LOGI(TAG, "LED turned OFF");
        vTaskDelay(1000 / portTICK_PERIOD_MS);  
        
    }

   
}
