#include "webSocket_manager.h"
#include "config.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "protocol_manager.h"

static const char *TAG = "WS_MANAGER";
static esp_websocket_client_handle_t ws_client = NULL;
static bool is_connected = false;

// WebSocket Event Handler (Bağlantı durumlarını ve gelen yanıtları yakalar)
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Python API WebSocket sunucusuna baglanildi.");
            is_connected = true;
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket baglantisi koptu.");
            is_connected = false;
            break;
            
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                protocol_manager_parse((const char *)data->data_ptr, data->data_len);
            } 
            else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
                // Python'dan gelen ses verisini direkt hoparlör modülüne basıyoruz
                audio_output_play((const uint8_t *)data->data_ptr, data->data_len);
            }
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket hatasi meydana geldi.");
            break;
    }
}

esp_err_t websocket_manager_init(void) {
    if (ws_client != NULL) {
        ESP_LOGW(TAG, "WebSocket zaten initialize edilmis.");
        return ESP_OK;
    }

    // ESP-IDF 5.x uyumlu güncel config yapısı
    esp_websocket_client_config_t ws_cfg = {
        .uri = WEBSOCKET_URI,
        .buffer_size = 2048, 
    };

    ws_client = esp_websocket_client_init(&ws_cfg);
    if (ws_client == NULL) {
        ESP_LOGE(TAG, "WebSocket istemcisi olusturulamadi.");
        return ESP_FAIL;
    }

    // Event handler kaydı
    esp_err_t err = esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Event kaydi basarisiz: %s", esp_err_to_name(err));
        return err;
    }

    // Arka planda çalışan WebSocket taskını tetikler
    err = esp_websocket_client_start(ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket baslatilamadi: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

void websocket_manager_stop(void) {
    if (ws_client != NULL) {
        esp_websocket_client_stop(ws_client);
        esp_websocket_client_destroy(ws_client);
        ws_client = NULL;
        is_connected = false;
        ESP_LOGI(TAG, "WebSocket istemcisi kapatildi ve temizlendi.");
    }
}

int websocket_manager_send_bin(const uint8_t *data, size_t len) {
    if (!is_connected || ws_client == NULL) {
        ESP_LOGW(TAG, "Veri gonderilemedi: Baglanti aktif degil.");
        return -1;
    }
    
    return esp_websocket_client_send_bin(ws_client, (const char *)data, len, portMAX_DELAY);
}

// 🚀 Yeni Eklenen Metin/Komut Gönderme Fonksiyonu
int websocket_manager_send_text(const char *text) {
    if (!is_connected || ws_client == NULL) {
        ESP_LOGW(TAG, "Metin gonderilemedi: Baglanti aktif degil.");
        return -1;
    }
    
    return esp_websocket_client_send_text(ws_client, text, strlen(text), portMAX_DELAY);
}

bool websocket_manager_is_connected(void) {
    return is_connected;
}