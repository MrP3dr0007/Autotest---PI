/* ioexp.h - Expansor PCF8574 #2 (0x21): /EN dos muxes (P0,P1), CS do SPI (P2) */
#ifndef IOEXP_H
#define IOEXP_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

void      ioexp_init(void);
bool      ioexp_presente(void);
/* Altera so os bits de 'mascara', mantendo os demais (sombra protegida por mutex). */
esp_err_t ioexp_set(uint8_t mascara, uint8_t valor);

#endif
