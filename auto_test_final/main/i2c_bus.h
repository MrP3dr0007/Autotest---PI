/* i2c_bus.h - Barramento I2C compartilhado (mestre, GPIO4/GPIO5) */
#ifndef I2C_BUS_H
#define I2C_BUS_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define I2C_MAX_ACHADOS 16

esp_err_t i2c_bus_init(void);
esp_err_t i2c_bus_ler16(uint8_t addr, uint8_t reg, uint16_t *valor);
esp_err_t i2c_bus_escrever16(uint8_t addr, uint8_t reg, uint16_t valor);
esp_err_t i2c_bus_escrever8(uint8_t addr, uint8_t valor);
/* addr, byte de controle, ate 32 bytes de dados (ex.: SSD1306: 0x00 comando, 0x40 dados) */
esp_err_t i2c_bus_escrever_ctrl(uint8_t addr, uint8_t ctrl, const uint8_t *dados, int n);   /* PCF8574 e similares */
bool      i2c_bus_responde(uint8_t addr);
/* Varre 0x08..0x77. Devolve quantos enderecos responderam (ate max). */
int       i2c_bus_varrer(uint8_t *achados, int max);

#endif
