#include "protocol_manager.h"
#include "esp_log.h"
#include "cJSON.h" // ESP-IDF ile gömülü gelen JSON kütüphanesi
#include <string.h>

static const char *TAG = "PROTOCOL_MANAGER";

esp_err_t protocol_manager_parse(const char *data, size_t len) {
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // JSON metnini belleğe alıp parse ediyoruz
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON Ayristirma Hatasi! Ham veri: %.*s", (int)len, data);
        return ESP_FAIL;
    }

    // Paket tipini ayırt etmek için "type" alanını kontrol et
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (cJSON_IsString(type_item) && (type_item->valuestring != NULL)) {
        const char *type = type_item->valuestring;
        ESP_LOGI(TAG, "Gelen Paket Tipi: %s", type);

        // Senaryo 1: Sesimizin yazıya dökülmüş hali (Faster-Whisper sonucu)
        if (strcmp(type, "transcript") == 0) {
            cJSON *text_item = cJSON_GetObjectItemCaseSensitive(root, "text");
            if (cJSON_IsString(text_item) && (text_item->valuestring != NULL)) {
                ESP_LOGI(TAG, "Anlasilan Metin: %s", text_item->valuestring);
            }
        } 
        // Senaryo 2: Sunucudan gelen bir restorasyon/otomasyon komutu
        else if (strcmp(type, "command") == 0) {
            cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(root, "payload");
            if (cJSON_IsString(cmd_item) && (cmd_item->valuestring != NULL)) {
                ESP_LOGI(TAG, "Calistirilacak Komut: %s", cmd_item->valuestring);
                // İleride buraya garson çağrı ledleri, ekran veya röle tetikleri bağlanabilir
            }
        } 
        // Senaryo 3: Sesli yanıt sistemi tetiklendiğinde (TTS)
        else if (strcmp(type, "tts_start") == 0) {
            ESP_LOGI(TAG, "Hoparlor hazirlaniyor, ses verisi akisi baslayacak...");
            // İleride buraya audio_output modülünü tetikleyecek kodu koyacağız
        } 
        else {
            ESP_LOGW(TAG, "Bilinmeyen paket tipi alındı: %s", type);
        }
    } else {
        ESP_LOGE(TAG, "Gecersiz protokol: 'type' alani bulunamadi.");
    }

    // Bellek sızıntısını önlemek için JSON nesnesini temizle
    cJSON_Delete(root);
    return ESP_OK;
}