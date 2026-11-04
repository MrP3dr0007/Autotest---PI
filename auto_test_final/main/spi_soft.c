/*
 * spi_soft.c - SPI por bit-bang (modo 0: repouso SCLK=0, amostra na subida)
 * Pinos: SCLK=GPIO13, MOSI=GPIO15, MISO=GPIO12. Logica 3,3 V SOMENTE.
 * Velocidade tipica de ~100-200 kHz; suficiente para ler IDs e registradores.
 */
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "spi_soft.h"
#include "ioexp.h"
#include "config.h"

static void pino(int gpio, gpio_mode_t modo)
{
    gpio_config_t c;

    c.pin_bit_mask = (1ULL << gpio);
    c.mode = modo;
    c.pull_up_en = GPIO_PULLUP_DISABLE;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&c);
}

static void liberar(void)
{
    pino(PIN_SPI_SCLK, GPIO_MODE_INPUT);
    pino(PIN_SPI_MOSI, GPIO_MODE_INPUT);
    pino(PIN_SPI_MISO, GPIO_MODE_INPUT);
}

esp_err_t spi_soft_transferir(const uint8_t *tx, uint8_t *rx, size_t n)
{
    esp_err_t e;

    if (tx == NULL || rx == NULL || n == 0 || n > 16) {
        return ESP_ERR_INVALID_STATE;
    }
    pino(PIN_SPI_SCLK, GPIO_MODE_OUTPUT);
    pino(PIN_SPI_MOSI, GPIO_MODE_OUTPUT);
    pino(PIN_SPI_MISO, GPIO_MODE_INPUT);
    gpio_set_level(PIN_SPI_SCLK, 0);
    gpio_set_level(PIN_SPI_MOSI, 0);

    e = ioexp_set(IOEXP_BIT_CS_SPI, 0x00);              /* CS em BAIXO */
    if (e != ESP_OK) {
        liberar();
        return e;
    }
    ets_delay_us(5);
    for (size_t k = 0; k < n; k++) {
        uint8_t entrada = 0;

        for (int bit = 7; bit >= 0; bit--) {
            gpio_set_level(PIN_SPI_MOSI, (tx[k] >> bit) & 1);
            ets_delay_us(SPI_MEIO_PERIODO_US);
            gpio_set_level(PIN_SPI_SCLK, 1);
            ets_delay_us(SPI_MEIO_PERIODO_US);
            entrada = (uint8_t)((entrada << 1) | (gpio_get_level(PIN_SPI_MISO) ? 1 : 0));
            gpio_set_level(PIN_SPI_SCLK, 0);
        }
        rx[k] = entrada;
    }
    ets_delay_us(5);
    ioexp_set(IOEXP_BIT_CS_SPI, IOEXP_BIT_CS_SPI);       /* CS em ALTO */
    liberar();
    return ESP_OK;
}
