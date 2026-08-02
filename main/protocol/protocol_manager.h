#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#include "esp_err.h"
#include <stddef.h>

// WebSocket'ten gelen JSON formatındaki metin verilerini ayrıştırır ve işler
esp_err_t protocol_manager_parse(const char *data, size_t len);

#endif // PROTOCOL_MANAGER_H