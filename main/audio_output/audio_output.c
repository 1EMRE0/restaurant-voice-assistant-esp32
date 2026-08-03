#include "audio_output.h"
#include "config.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "AUDIO_OUTPUT";
static i2s_chan_handle_t tx_handle = NULL; // ESP-IDF 5.x TX Kanal yapısı
static bool is_playing = false;

esp_err_t audio_output_init(void) {
    if (tx_handle != NULL) {
        ESP_LOGW(TAG, "I2S TX zaten initialize edilmis.");
        return ESP_OK;
    }

    // 1. Kanal Konfigürasyonu (I2S_NUM_1, Master Mod, Sadece TX/Verici)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle, NULL); 
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX kanali olusturulamadi: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Standart Mod Konfigürasyonu
    // Düzeltilmiş slot konfigürasyonu:
i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(AUDIO_BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO), // 🛠️ MSB yerine PHILIPS yapıldı
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = I2S_TX_BCLK_PIN,
        .ws = I2S_TX_WS_PIN,
        .dout = I2S_TX_DOUT_PIN,
        .din = I2S_GPIO_UNUSED,
    },
};

    err = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX standart mod yuklenemedi: %s", esp_err_to_name(err));
        return err;
    }

    // 🚀 3. Kanalı Doğrudan Aktif Et (Hoparlörü Dinlemeye Hazırla)
    err = i2s_channel_enable(tx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S TX kanal enable basarisiz: %s", esp_err_to_name(err));
        return err;
    }
    is_playing = true;

    ESP_LOGI(TAG, "I2S Ses cikis modulu basariyla hazirlandi ve aktiflestirildi.");
    return ESP_OK;
}

esp_err_t audio_output_start(void) {
    if (tx_handle == NULL) return ESP_ERR_INVALID_STATE;
    
    if (!is_playing) {
        esp_err_t err = i2s_channel_enable(tx_handle);
        if (err == ESP_OK) {
            is_playing = true;
            ESP_LOGI(TAG, "Hoparlor kanali acildi.");
        }
        return err;
    }
    return ESP_OK;
}

esp_err_t audio_output_stop(void) {
    if (tx_handle == NULL) return ESP_ERR_INVALID_STATE;

    if (is_playing) {
        is_playing = false;
        esp_err_t err = i2s_channel_disable(tx_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Hoparlor kanali kapatildi.");
        }
        return err;
    }
    return ESP_OK;
}

int audio_output_play(const uint8_t *data, size_t len) {
    if (!is_playing || tx_handle == NULL) {
        ESP_LOGW(TAG, "Ses oynatılamadı: Hoparlor kanali aktif degil.");
        return -1;
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(
    tx_handle,
    data,
    len,
    &bytes_written,
    portMAX_DELAY
);

if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2S yazma hatasi: %s", esp_err_to_name(err));
    return -1;
}

if (bytes_written != len) {
    ESP_LOGW(TAG,
             "Eksik ses verisi yazildi (%u / %u byte)",
             (unsigned)bytes_written,
             (unsigned)len);
}

return (int)bytes_written;
}