/* test_manager.h - Maquina de estados e execucao dos testes */
#ifndef TEST_MANAGER_H
#define TEST_MANAGER_H
#include <stdint.h>
#include <stdbool.h>
#include "measure.h"

typedef enum {
    RES_PENDENTE = 0,
    RES_EXECUTANDO,
    RES_AGUARDANDO_OPERADOR,
    RES_APROVADO,
    RES_REPROVADO,
    RES_INCONCLUSIVO,
    RES_INTERROMPIDO,
    RES_NAO_APLICAVEL
} resultado_t;

typedef enum { MODO_ASSISTIDO = 0, MODO_AUTOMATICO } modo_t;

typedef enum {
    TESTE_SISTEMA = 0,
    TESTE_LED,
    TESTE_BOTAO,
    TESTE_I2C,
    TESTE_MEDIDOR,
    TESTE_TENSAO,
    TESTE_CORRENTE,
    TESTE_CONTINUIDADE,
    TESTE_PONTOS,
    TESTE_VARREDURA,
    TESTE_I2C_REG,
    TESTE_SPI_ID,
    TESTE_UART,
    TESTE_OLED,
    NUM_TESTES
} teste_id_t;

typedef enum { SESSAO_OCIOSA = 0, SESSAO_EXECUTANDO, SESSAO_CONCLUIDA } sessao_estado_t;

typedef enum {
    TM_OK = 0,
    TM_ERRO_OCUPADO,
    TM_ERRO_SEGURANCA,
    TM_ERRO_PERFIL,
    TM_ERRO_MODO_BLOQUEADO,
    TM_ERRO_INTERNO
} tm_erro_t;

typedef struct {
    const char *nome;
    resultado_t resultado;
    char        detalhe[120];
} teste_t;

typedef struct {
    sessao_estado_t estado;
    modo_t          modo;
    int             perfil;
    resultado_t     geral;
    teste_t         testes[NUM_TESTES];
    uint32_t        inicio_ms;
    uint32_t        fim_ms;
    bool            med_valida;      /* houve leitura real no fim da sessao */
    int32_t         v_mv;
    int32_t         i_ma;
    limites_t       lim;             /* limites usados na sessao (para o relatorio) */
    uint32_t        epoch;           /* data real (0 = relogio nao ajustado) */
    bool            aguardando;
    char            pergunta[96];
} sessao_t;

void        test_manager_init(void);
bool        test_manager_ocupado(void);
tm_erro_t   test_manager_iniciar(int perfil, modo_t modo);
void        test_manager_confirmar(bool sim);
bool        test_manager_rearmar(void);
void        test_manager_copiar(sessao_t *destino);
uint32_t    test_manager_agora_ms(void);

const char *test_manager_nome(int teste);
const char *resultado_texto(resultado_t r);
const char *resultado_codigo(resultado_t r);
const char *modo_texto(modo_t m);
const char *tm_erro_texto(tm_erro_t e);

#endif
