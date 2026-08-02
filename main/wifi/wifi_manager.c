#include "wifi_manager.h"
#include "esp_timer.h"
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* SSID/parolayı kaynak koda gömmek yerine Kconfig'ten al:
 * idf.py menuconfig -> "Wifi Manager Configuration" altına
 * bu satırları içeren bir Kconfig.projbuild dosyası eklenmeli. */
#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID      "Emre"
#endif
#ifndef CONFIG_WIFI_PASSWORD
#define CONFIG_WIFI_PASSWORD  "123456789"
#endif

#define WIFI_MAX_RETRY         10
#define WIFI_RETRY_BASE_MS     1000    // ilk retry gecikmesi
#define WIFI_RETRY_MAX_MS      30000   // üst sınır (exponential backoff)

#define WIFI_CONNECTED_BIT     BIT0
#define WIFI_FAIL_BIT          BIT1

static const char *TAG = "WIFI";

static EventGroupHandle_t s_wifi_event_group;
static esp_timer_handle_t  s_retry_timer;
static int                 s_retry_count = 0;
static int                 s_rssi        = 0;

static void schedule_retry(void);

static void retry_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Reconnect attempt %d/%d", s_retry_count + 1, WIFI_MAX_RETRY);
    esp_wifi_connect();
}

static void schedule_retry(void)
{
    if (s_retry_count >= WIFI_MAX_RETRY) {
        ESP_LOGE(TAG, "Max retry reached, giving up");
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        return;
    }

    // Exponential backoff: 1s, 2s, 4s, 8s ... WIFI_RETRY_MAX_MS'e kadar
    uint32_t delay_ms = WIFI_RETRY_BASE_MS << s_retry_count;
    if (delay_ms > WIFI_RETRY_MAX_MS || delay_ms == 0) {
        delay_ms = WIFI_RETRY_MAX_MS;
    }
    s_retry_count++;

    ESP_LOGW(TAG, "Retrying in %lu ms", (unsigned long)delay_ms);
    esp_timer_start_once(s_retry_timer, (uint64_t)delay_ms * 1000);
}

static const char *disconnect_reason_str(uint8_t reason)
{
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:        return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_FAIL:          return "AUTH_FAIL (yanlis sifre olabilir)";
        case WIFI_REASON_NO_AP_FOUND:        return "NO_AP_FOUND";
        case WIFI_REASON_ASSOC_LEAVE:        return "ASSOC_LEAVE";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:  return "HANDSHAKE_TIMEOUT";
        default:                             return "OTHER";
    }
}

static void wifi_event_handler(void *arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Station started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                ESP_LOGW(TAG, "Disconnected, reason: %s (%d)",
                         disconnect_reason_str(disc->reason), disc->reason);
                schedule_retry();
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        s_retry_count = 0; // basarili baglanti, sayaci sifirla
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_rssi = ap_info.rssi;
        }

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erase & reinit gerekiyor");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        return ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &retry_timer_cb,
        .name = "wifi_retry"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_retry_timer));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            // Güvenlik: WPA2'nin altına düşmeyi engelle (downgrade saldırılarına karşı)
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable  = true,
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Performans: gecikmeye duyarli uygulamalar icin power-save kapali.
    // Pil ile calisiyorsan WIFI_PS_MIN_MODEM kullan.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init tamamlandi, baglaniliyor: %s", CONFIG_WIFI_SSID);
    return ESP_OK;
}

bool wifi_wait_connected(uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,   // bitleri temizleme
        pdFALSE,   // herhangi biri yeterli (OR)
        ticks);

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_is_connected(void)
{
    if (!s_wifi_event_group) return false;
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

int wifi_get_rssi(void)
{
    return wifi_is_connected() ? s_rssi : 0;
}