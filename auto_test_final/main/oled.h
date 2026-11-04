/* oled.h - Painel OLED SSD1306 por I2C (teste visual) */
#ifndef OLED_H
#define OLED_H
#include <stdint.h>
#include "esp_err.h"

typedef enum { OLED_TUDO_ACESO = 0, OLED_APAGADO, OLED_XADREZ } oled_padrao_t;

esp_err_t oled_iniciar(uint8_t addr);
esp_err_t oled_mostrar(uint8_t addr, uint8_t paginas, oled_padrao_t padrao);
void      oled_desligar(uint8_t addr);

#endif
