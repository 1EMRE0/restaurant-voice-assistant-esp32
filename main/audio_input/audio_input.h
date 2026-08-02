#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include "esp_err.h"

// I2S donanımını ve ses yakalama taskını başlatır
esp_err_t audio_input_init(void);

// Akışı (Streaming) başlatır (Butona basıldığında çağrılacak)
esp_err_t audio_input_start_streaming(void);

// Akışı durdurur (Buton bırakıldığında çağrılacak)
esp_err_t audio_input_stop_streaming(void);

#endif // AUDIO_INPUT_H