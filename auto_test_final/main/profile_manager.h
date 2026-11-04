/* profile_manager.h - Perfis de teste (placa, limites, sequencia autorizada) */
#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H
#include <stdbool.h>
#include <stdint.h>
#include "test_manager.h"
#include "measure.h"

#define MASCARA(id) (1u << (id))
#define PERFIL_MAX_I2C 4
#define PERFIL_MAX_PONTOS 8
#define PERFIL_MAX_I2C_REGS 3

/* Ponto de teste do adaptador: canal 'a' (mux A) x canal 'b' (mux B). */
typedef struct {
    const char *nome;
    uint8_t     a, b;
    int32_t     rmin_ohm, rmax_ohm;   /* faixa esperada (rmax=0: sem maximo)   */
    bool        aberto;               /* true: o esperado e circuito ABERTO     */
} ponto_t;

/* Registrador I2C de um dispositivo conhecido (leitura de 16 bits). */
typedef struct {
    const char *nome;                 /* NULL = fim da lista */
    uint8_t     addr, reg;
    uint16_t    esperado, mascara;    /* (lido & mascara) == (esperado & mascara) */
} i2c_reg_t;

typedef struct {
    const char *id;
    const char *nome;
    const char *descricao;
    const char *alertas;
    bool        validado;             /* so perfil validado libera o modo automatico */
    uint16_t    testes_assistido;     /* mascara de teste_id_t */
    uint16_t    testes_automatico;
    limites_t   limites_padrao;       /* sugestao carregada no painel (mV / mA) */
    uint8_t     i2c_esperados[PERFIL_MAX_I2C];   /* 0 = fim da lista */
    ponto_t     pontos[PERFIL_MAX_PONTOS];       /* nome NULL = fim da lista */
    bool        varredura_definida;              /* true: ha contagem esperada */
    uint8_t     condutivos_esperados;            /* pares A x B abaixo do limiar */
    i2c_reg_t   i2c_regs[PERFIL_MAX_I2C_REGS];   /* dispositivos I2C conhecidos */
    uint8_t     spi_cmd;                         /* comando de leitura de ID (0 = sem teste SPI) */
    uint8_t     spi_n;                           /* bytes de ID esperados (1..3) */
    uint8_t     spi_esperado[3];
    uint32_t    uart_baud;                       /* 0 = sem teste UART */
    uint8_t     oled_addr;                       /* 0 = sem teste de tela */
    uint8_t     oled_paginas;                    /* 8 (128x64) ou 4 (128x32) */
} perfil_t;

int            perfil_total(void);
const perfil_t *perfil_obter(int indice);

#endif
