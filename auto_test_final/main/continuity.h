/* continuity.h - Teste de continuidade / resistencia com a placa DESENERGIZADA */
#ifndef CONTINUITY_H
#define CONTINUITY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { CONT_ERRO = 0, CONT_ABERTO, CONT_VALOR } cont_estado_t;

void          continuity_init(void);
/* Mede entre a ponta A (GPIO14 via R1) e a ponta B (A0). Em CONT_VALOR,
 * *r_ohm recebe a resistencia estimada (>= 0). So chamar com a carga
 * desenergizada: quem chama deve garantir isso. */
cont_estado_t continuity_medir(int32_t *r_ohm);
/* Versao com numero de amostras; rapido = espera por microssegundos (varredura). */
cont_estado_t continuity_medir_n(int amostras, bool rapido, int32_t *r_ohm);

#endif
