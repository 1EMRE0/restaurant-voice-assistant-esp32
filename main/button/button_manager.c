#include "button_manager.h"
#include "config.h"
#include "audio_input.h" 
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "BUTTON_MANAGER";
static QueueHandle_t button_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(button_evt_queue, &gpio_num, NULL);
}

static void button_task(void* arg) {
    uint32_t io_num;
    bool last_state = true; 

    for (;;) {
        if (xQueueReceive(button_evt_queue, &io_num, portMAX_DELAY)) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Basit debounce gecikmesi
            int current_state = gpio_get_level(io_num);

            if (current_state != last_state) {
                last_state = current_state;
                
                if (current_state == 0) { 
                    ESP_LOGI(TAG, "Butona basildi -> Ses kaydi basliyor.");
                    audio_input_start_streaming(); // Ses akışını başlat ve sunucuya gönder
                } else { 
                    ESP_LOGI(TAG, "Buton birakildi -> Ses kaydi durduruluyor.");
                    audio_input_stop_streaming(); // Ses akışını durdur
                }
            }
        }
    }
}

esp_err_t button_manager_init(void) {
    button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (button_evt_queue == NULL) {
        ESP_LOGE(TAG, "Buton event kuyruğu olusturulamadi.");
        return ESP_FAIL;
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,          // Hem basılma hem bırakılma anı
        .mode = GPIO_MODE_INPUT,                 
        .pin_bit_mask = (1ULL << BUTTON_GPIO),   // config.h içerisindeki pin numaran
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE         // Dahili pull-up aktif
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO konfigürasyonu basarisiz: %s", esp_err_to_name(err));
        return err;
    }

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    err = gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Buton ISR handler eklenemedi: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t task_err = xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
    if (task_err != pdPASS) {
        ESP_LOGE(TAG, "Buton yonetim taskı olusturulamadi.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Buton modülü (Push-to-Talk) basariyla hazirlandi.");
    return ESP_OK;
}