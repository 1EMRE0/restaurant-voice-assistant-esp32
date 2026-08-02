#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// I2S çıkış kanalını hazırla
esp_err_t audio_output_init(void);

// Hoparlör kanalını aktifleştirir
esp_err_t audio_output_start(void);

// Hoparlör kanalını kapatır (enerji tasarrufu ve tıslama önleme için)
esp_err_t audio_output_stop(void);

// Gelen ses verisini (PCM) DMA üzerinden hoparlöre basar
int audio_output_play(const uint8_t *data, size_t len);

#endif // AUDIO_OUTPUT_H