/* power.h - Controle da saida de potencia (chave em GPIO16) */
#ifndef POWER_H
#define POWER_H
#include <stdbool.h>

bool        power_instalada(void);
bool        power_ligada(void);
/* Devolve NULL se ligou; senao o motivo da recusa. */
const char *power_ligar(void);
void        power_desligar(void);

#endif
