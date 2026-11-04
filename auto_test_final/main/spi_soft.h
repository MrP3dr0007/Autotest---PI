/* spi_soft.h - SPI mestre por software (modo 0, MSB primeiro) */
#ifndef SPI_SOFT_H
#define SPI_SOFT_H
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Uma transacao completa com CS (PCF8574 #2, P2) em BAIXO. n <= 16.
 * Os pinos SCLK/MOSI/MISO voltam a alta impedancia no fim. */
esp_err_t spi_soft_transferir(const uint8_t *tx, uint8_t *rx, size_t n);

#endif
