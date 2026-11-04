/* result_logger.h - Historico dos diagnosticos (RAM + copia na flash/NVS) */
#ifndef RESULT_LOGGER_H
#define RESULT_LOGGER_H
#include <stdbool.h>
#include <stdint.h>
#include "test_manager.h"

typedef struct {
    uint32_t    id;
    uint32_t    uptime_s;            /* tempo desde a energizacao (zera no reinicio) */
    uint32_t    epoch;               /* data real se o navegador ajustou o relogio; 0 = sem data */
    char        perfil_id[16];
    modo_t      modo;
    resultado_t geral;
    resultado_t testes[NUM_TESTES];
    bool        med_valida;          /* so ha valores se houve leitura real */
    int32_t     v_mv;
    int32_t     i_ma;
    uint32_t    duracao_ms;
} registro_t;

void   result_logger_init(void);
void   result_logger_registrar(const sessao_t *s);
int    result_logger_total(void);
bool   result_logger_obter(int indice_recente, registro_t *saida);  /* 0 = mais recente */
bool   result_logger_por_id(uint32_t id, registro_t *saida);
/* Ultimo registro ANTERIOR a "antes_do_id", do mesmo perfil e com medicao real. */
bool   result_logger_anterior(const char *perfil_id, uint32_t antes_do_id, registro_t *saida);
uint32_t result_logger_ultimo_id(void);

#endif
