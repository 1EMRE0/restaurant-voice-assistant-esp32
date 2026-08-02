#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "esp_err.h"

// Buton sürücüsünü, kesme mekanizmasını ve yönetim taskını başlatır
esp_err_t button_manager_init(void);

#endif // BUTTON_MANAGER_H