/* relogio.h - Data/hora vindas do navegador (a placa nao tem RTC) */
#ifndef RELOGIO_H
#define RELOGIO_H
#include <stdbool.h>
#include <stdint.h>

bool     relogio_definir(uint32_t epoch_s);   /* false se o valor for implausivel */
uint32_t relogio_agora(void);                  /* 0 = relogio ainda nao ajustado */

#endif
