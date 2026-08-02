#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "audio_output.h"

// WebSocket istemcisini başlatır ve sunucuya bağlanır
esp_err_t websocket_manager_init(void);

// WebSocket bağlantısını güvenli şekilde kapatır
void websocket_manager_stop(void);

// I2S'ten gelen ham ses verilerini (binary) Python API'ye göndermek için
int websocket_manager_send_bin(const uint8_t *data, size_t len);

// Python API'ye metin/JSON komutları (örn: STREAM_END) göndermek için
int websocket_manager_send_text(const char *text);

// Bağlantının aktif olup olmadığını kontrol eder
bool websocket_manager_is_connected(void);

#endif // WEBSOCKET_MANAGER_H