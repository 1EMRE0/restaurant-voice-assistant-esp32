#include "audio_input.h"
#include "config.h"
#include "webSocket_manager.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_INPUT";
static i2s_chan_handle_t rx_handle = NULL; // ESP-IDF 5.x Kanal Handle yapısı
static bool is_streaming = false;

#define RECORD_BUFFER_SIZE 1024 // I2S'ten okunacak ham (32-bit) veri boyutu, byte cinsinden

static void audio_rx_task(void *arg) {
    // 32-bit ham veri buffer'ı (I2S donanımı INMP441'den hep 32-bit slot okur)
    int32_t *raw_buffer = (int32_t *)malloc(RECORD_BUFFER_SIZE);
    // 16-bit dönüştürülmüş PCM buffer'ı (WebSocket / Whisper için, yarı boyut)
    int16_t *pcm16_buffer = (int16_t *)malloc(RECORD_BUFFER_SIZE / 2);

    if (raw_buffer == NULL || pcm16_buffer == NULL) {
        ESP_LOGE(TAG, "Ses buffer'i icin bellek ayrilamadi!");
        if (raw_buffer) free(raw_buffer);
        if (pcm16_buffer) free(pcm16_buffer);
        vTaskDelete(NULL);
        return;
    }

    size_t bytes_read = 0;

    for (;;) {
        if (is_streaming) {
            // I2S donanımından ham PCM verisini (32-bit) DMA üzerinden oku
            // Bu fonksiyon blocking çalışır, veri geldikçe döner
            esp_err_t err = i2s_channel_read(rx_handle, raw_buffer, RECORD_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

            if (err == ESP_OK && bytes_read > 0) {
                int samples = bytes_read / sizeof(int32_t);

                // 32-bit -> 16-bit PCM dönüşümü
                // INMP441 verisi 32-bit slot içinde sola yaslı (MSB) gelir,
                // üst 16 biti alarak gerçek 16-bit örneği çıkarıyoruz.
                // Ses çok kısık/çok yüksek gelirse bu kaydırma miktarını
                // (>> 16) test ederek >> 14 veya >> 8 ile deneyebilirsiniz.
                for (int i = 0; i < samples; i++) {
                    pcm16_buffer[i] = (int16_t)(raw_buffer[i] >> 16);
                }

                // Eğer WebSocket bağlantısı aktifse dönüştürülmüş 16-bit veriyi gönder
                if (websocket_manager_is_connected()) {
                    websocket_manager_send_bin((uint8_t *)pcm16_buffer, samples * sizeof(int16_t));
                }
            } else if (err != ESP_OK) {
                ESP_LOGE(TAG, "I2S okuma hatasi: %s", esp_err_to_name(err));
            }
        } else {
            // Akış kapalıysa taskı uyut, CPU'yu boşa yorma
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    free(raw_buffer);
    free(pcm16_buffer);
    vTaskDelete(NULL);
}

esp_err_t audio_input_init(void) {
    if (rx_handle != NULL) {
        ESP_LOGW(TAG, "I2S zaten initialize edilmis.");
        return ESP_OK;
    }

    // 1. Kanal Konfigürasyonu (I2S_NUM_0, Master Mod, Sadece RX/Alıcı)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S kanali olusturulamadi: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Standart Mod Konfigürasyonu (Clock ve Slot Ayarları)
    // ÖNEMLİ: INMP441 standart I2S (Philips) protokolü kullanır, MSB değil.
    // MSB formatı kullanılırsa veri 1 bit kayar ve ses bozuk/cızırtılı gelir.
    // Donanım okuma genişliği 32-bit olmalı (INMP441 24-bit veriyi 32-bit
    // slot içinde sola yaslı gönderir); 16-bit'e dönüşüm yazılımda yapılır.
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // Dijital mikrofonlarda genelde MCLK gerekmez
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DIN_PIN,
        },
    };

    // Ayarları kanala yükle
    err = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S standart mod yuklenemedi: %s", esp_err_to_name(err));
        return err;
    }

    // 3. Arka planda sürekli DMA dinleyecek yüksek öncelikli ses taskını oluştur
    BaseType_t task_err = xTaskCreatePinnedToCore(audio_rx_task, "audio_rx_task", 4096, NULL, 10, NULL, 1);
    if (task_err != pdPASS) {
        ESP_LOGE(TAG, "Ses yakalama taski olusturulamadi.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2S Ses giris modülü basariyla hazirlandi.");
    return ESP_OK;
}

esp_err_t audio_input_start_streaming(void) {
    if (rx_handle == NULL) return ESP_ERR_INVALID_STATE;

    if (!is_streaming) {
        // I2S donanımsal DMA kanalını aktifleştirir
        esp_err_t err = i2s_channel_enable(rx_handle);
        if (err == ESP_OK) {
            is_streaming = true;
            ESP_LOGI(TAG, "Ses akisi baslatildi.");
        }
        return err;
    }
    return ESP_OK;
}

esp_err_t audio_input_stop_streaming(void) {
    if (rx_handle == NULL) return ESP_ERR_INVALID_STATE;

    if (is_streaming) {
        is_streaming = false;
        // Donanımsal kanalı kapatır, buffer'ları temizler
        esp_err_t err = i2s_channel_disable(rx_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Ses akisi durduruldu.");

            // Python sunucusuna kayıt bitti sinyalini gönder
            if (websocket_manager_is_connected()) {
                websocket_manager_send_text("{\"event\": \"STREAM_END\"}");
                ESP_LOGI(TAG, "Python sunucusuna STREAM_END sinyali gonderildi.");
            }
        }
        return err;
    }
    return ESP_OK;
}