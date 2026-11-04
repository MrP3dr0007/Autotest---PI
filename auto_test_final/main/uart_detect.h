/* uart_detect.h - Detector de baud por software (EXPERIMENTAL) */
#ifndef UART_DETECT_H
#define UART_DETECT_H
#include <stdint.h>

typedef struct {
    int      pulsos;        /* pulsos em BAIXO observados */
    uint32_t menor_us;      /* menor pulso observado (aprox. 1 bit) */
    uint32_t baud;          /* baud padrao mais proximo; 0 = nao classificado */
    int      limite;        /* 1 = menor pulso no limite da resolucao do detector */
} uart_medida_t;

/* Fica ate 'janela_ms' observando o pino. Desabilita interrupcoes por ~7 ms por
 * vez (o Wi-Fi pode perder um quadro; nao use durante outros testes). */
void uart_detectar(int janela_ms, uart_medida_t *m);

#endif
