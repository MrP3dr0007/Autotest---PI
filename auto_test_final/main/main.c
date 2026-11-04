/*
 * AUTO TEST - Bancada modular de diagnostico (Versao 1.0 (etapas 1 a 7))
 * Firmware em C puro para ESP8266 (NodeMCU / ESP-12) com ESP8266_RTOS_SDK v3.4.
 *
 * Painel: conecte-se ao Wi-Fi "AutoTest-ESP" e abra http://192.168.4.1/
 */
#include "esp_log.h"
#include "nvs_flash.h"
#include "board_io.h"
#include "i2c_bus.h"
#include "measure.h"
#include "continuity.h"
#include "matrix.h"
#include "relogio.h"
#include "power.h"
#include "safety.h"
#include "profile_manager.h"
#include "result_logger.h"
#include "test_manager.h"
#include "wifi_ap.h"
#include "web_interface.h"
#include "config.h"

static const char *TAG = "AUTO_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "Auto Test %s iniciando...", FW_VERSAO);

    ESP_ERROR_CHECK(nvs_flash_init());
    board_io_init();
    safety_init();
    if (i2c_bus_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C nao iniciou: medicao indisponivel.");
    }
    measure_init();
    continuity_init();
    matrix_init();
    result_logger_init();
    test_manager_init();
    wifi_ap_iniciar();
    web_interface_iniciar();

    ESP_LOGI(TAG, "Pronto. Rede %s, painel em http://192.168.4.1/", WIFI_SSID);
}
