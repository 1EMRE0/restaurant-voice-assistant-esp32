#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// config.h içine eklenecek pinler
#define I2S_BCLK_PIN     GPIO_NUM_33   // Serial Clock (SCK)
#define I2S_WS_PIN       GPIO_NUM_5   // Word Select (WS / LRC)
#define I2S_DIN_PIN      GPIO_NUM_18  // Data In (SD / DATA)
// Wi-Fi Ayarları
#define WIFI_SSID       "Emre"
#define WIFI_PASS       "123456789"
#define WIFI_RETRY_MAX  5

// config.h içine eklenecek çıkış pinleri
#define I2S_TX_BCLK_PIN  GPIO_NUM_26  // Bit Clock (BCLK)
#define I2S_TX_WS_PIN    GPIO_NUM_25  // Word Select (LRC)
#define I2S_TX_DOUT_PIN  GPIO_NUM_22  // Data Out (DIN)

// WebSocket Sunucu Ayarları (Lokal Python API adresi)
#define DEVICE_ID      "masa_1"
#define SERVER_IP      "10.85.192.215"

#define WEBSOCKET_URI  "ws://" SERVER_IP ":8000/ws/v1/audio/" DEVICE_ID

// Ses (I2S) Konfigürasyonu (Faster-Whisper genelde 16kHz Mono ister)
#define AUDIO_SAMPLE_RATE      16000
#define AUDIO_BITS_PER_SAMPLE  16
#define AUDIO_CHANNELS         1

// GPIO Pin Tanımlamaları (Lehim gelince pinleri buraya göre bağlarsın)
#define BUTTON_GPIO     GPIO_NUM_14

#endif // CONFIG_H