/* matrix.h - Matriz de pontos de teste (2 x CD74HC4067 via 2 x PCF8574) */
#ifndef MATRIX_H
#define MATRIX_H
#include <stdbool.h>
#include <stdint.h>
#include "continuity.h"

void          matrix_init(void);
bool          matrix_presente(void);          /* os dois PCF8574 responderam */
/* Desabilita os dois multiplexadores (estado seguro). */
void          matrix_liberar(void);
/* Mede entre o canal 'a' (mux A) e o canal 'b' (mux B). SO com a placa
 * desenergizada: quem chama garante. Desconta MUX_OFFSET_OHM. Libera os muxes
 * no fim. rapido = menos amostras (varredura). */
cont_estado_t matrix_medir_par(uint8_t a, uint8_t b, bool rapido, int32_t *r_ohm);

#endif
