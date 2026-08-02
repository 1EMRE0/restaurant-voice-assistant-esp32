#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "webSocket_manager.h"
#include "button_manager.h"
#include "audio_input.h"
#include "audio_output.h"

static const char *TAG = "MAIN_APP";

void app_main(void) {
    ESP_LOGI(TAG, "Sistem baslatiliyor...");

    // 1. Wi-Fi Modülünü Başlat
    if (wifi_init() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi omurgasi baslatilamadi!");
        return;
    }

    // 2. Wi-Fi Bağlantısını ve IP Alınmasını Bekle
    ESP_LOGI(TAG, "IP adresi alinmasi bekleniyor...");
    if (wifi_wait_connected(15000)) {
        ESP_LOGI(TAG, "Wi-Fi baglantisi kuruldu, IP alindi. WebSocket baslatiliyor.");

        // 3. WebSocket İstemcisini Başlat
        if (websocket_manager_init() != ESP_OK) {
            ESP_LOGE(TAG, "WebSocket istemcisi kurulamadi!");
        }
    } else {
        ESP_LOGE(TAG, "Wi-Fi baglanti zaman asimi! WebSocket simdilik devre disi.");
    }

    // 4. Ses Giriş (I2S Mikrofon) Modülünü Hazırla
    if (audio_input_init() != ESP_OK) {
        ESP_LOGE(TAG, "Ses giris modulu kurulamadi!");
    }

    // 5. Ses Çıkış (I2S Hoparlör) Modülünü Hazırla
    if (audio_output_init() != ESP_OK) {
        ESP_LOGE(TAG, "Ses cikis modulu kurulamadi!");
    }

    // 6. Buton (Push-to-Talk) Modülünü Başlat
    if (button_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Buton yonetim modulu kurulamadi!");
    }

    ESP_LOGI(TAG, "Tum moduller yuklendi. Cihaz aktif dongude.");

    // Ana taskın canlı kalması için sonsuz döngü
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}