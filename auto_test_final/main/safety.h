/* safety.h - Estado de seguranca da bancada (latched) */
#ifndef SAFETY_H
#define SAFETY_H
#include <stdbool.h>

typedef enum {
    SEG_OK = 0,
    SEG_PARADA_EMERGENCIA,
    SEG_FALHA
} seguranca_estado_t;

typedef enum {
    SEG_MOTIVO_NENHUM = 0,
    SEG_MOTIVO_PARADA_OPERADOR,
    SEG_MOTIVO_MEMORIA_BAIXA,
    SEG_MOTIVO_SOBRETENSAO,
    SEG_MOTIVO_SOBRECORRENTE
} seguranca_motivo_t;

void               safety_init(void);
bool               safety_ok(void);
void               safety_disparar(seguranca_motivo_t motivo);
bool               safety_rearmar(void);
seguranca_estado_t safety_estado(void);
const char        *safety_motivo_texto(void);

#endif
