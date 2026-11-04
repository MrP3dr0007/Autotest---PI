/*
 * ioexp.c - Estado compartilhado do PCF8574 #2
 * O PCF8574 liga com todas as saidas em ALTO: muxes desabilitados e CS do SPI
 * desativado. Como varios modulos mexem em bits diferentes do mesmo byte,
 * mantemos uma "sombra" e so alteramos os bits pedidos.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ioexp.h"
#include "i2c_bus.h"
#include "config.h"

static SemaphoreHandle_t g_mutex;
static uint8_t g_sombra = 0xFF;
static bool    g_presente;

void ioexp_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    g_sombra = 0xFF;
    g_presente = i2c_bus_responde(MATRIZ_PCF_EN);
    if (g_presente) {
        i2c_bus_escrever8(MATRIZ_PCF_EN, g_sombra);
    }
}

bool ioexp_presente(void)
{
    return g_presente;
}

esp_err_t ioexp_set(uint8_t mascara, uint8_t valor)
{
    esp_err_t e;
    uint8_t novo;

    if (!g_presente) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    novo = (uint8_t)((g_sombra & ~mascara) | (valor & mascara));
    e = i2c_bus_escrever8(MATRIZ_PCF_EN, novo);
    if (e == ESP_OK) {
        g_sombra = novo;
    }
    xSemaphoreGive(g_mutex);
    return e;
}
