/* board_io.h - Acesso aos GPIOs da placa (LED e botao) */
#ifndef BOARD_IO_H
#define BOARD_IO_H
#include <stdbool.h>

void board_io_init(void);
void board_io_estado_seguro(void);     /* solta todas as saidas (alta impedancia) */
void board_led_set(bool aceso);
bool board_botao_pressionado(void);

/* Chave de potencia (GPIO16). Sem modulo instalado, nao faz nada. */
void board_potencia_set(bool ligada);
bool board_potencia_ligada(void);

#endif
