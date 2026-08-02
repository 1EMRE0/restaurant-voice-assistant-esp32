#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wi-Fi'yi başlatır ve bağlanmayı dener (non-blocking).
 *        SSID/parola Kconfig üzerinden (idf.py menuconfig) okunur.
 */
esp_err_t wifi_init(void);

/**
 * @brief Bağlantı kurulana (veya timeout'a) kadar bekler.
 * @param timeout_ms Maksimum bekleme süresi (ms), 0 = sonsuz bekle
 * @return true bağlandıysa, false timeout olduysa
 */
bool wifi_wait_connected(uint32_t timeout_ms);

/** @brief Anlık bağlantı durumunu döner (thread-safe, non-blocking). */
bool wifi_is_connected(void);

/** @brief Bağlı olduğu AP'nin RSSI değerini döner (dBm). Bağlı değilse 0 döner. */
int wifi_get_rssi(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H