/* wifi_ap.c - Ponto de acesso Wi-Fi (ESP8266_RTOS_SDK v3.4, API em C) */
#include <string.h>
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "wifi_ap.h"
#include "config.h"

static const char *TAG = "WIFI";

static esp_err_t tratar_evento(void *ctx, system_event_t *evento)
{
    (void)ctx;
    switch (evento->event_id) {
    case SYSTEM_EVENT_AP_START:          ESP_LOGI(TAG, "Ponto de acesso iniciado."); break;
    case SYSTEM_EVENT_AP_STACONNECTED:   ESP_LOGI(TAG, "Cliente conectado."); break;
    case SYSTEM_EVENT_AP_STADISCONNECTED: ESP_LOGI(TAG, "Cliente desconectado."); break;
    default: break;
    }
    return ESP_OK;
}

void wifi_ap_iniciar(void)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t cfg;

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(tratar_evento, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    memset(&cfg, 0, sizeof(cfg));
    strcpy((char *)cfg.ap.ssid, WIFI_SSID);
    strcpy((char *)cfg.ap.password, WIFI_SENHA);
    cfg.ap.ssid_len       = strlen(WIFI_SSID);
    cfg.ap.max_connection = WIFI_MAX_CLIENTES;
    cfg.ap.authmode       = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi: SSID=%s senha=%s", WIFI_SSID, WIFI_SENHA);
}
