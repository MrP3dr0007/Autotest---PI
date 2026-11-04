/*
 * matrix.c - Matriz de pontos de teste
 *
 * Ordem de comutacao (evita ligar canais errados durante a troca):
 *   1) desabilita os dois muxes   2) troca os enderecos   3) habilita
 * Os canais dos muxes so aceitam sinais entre 0 V e a alimentacao deles
 * (3,3 V). Use resistor serie em cada canal e SO meça placa desenergizada.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "matrix.h"
#include "i2c_bus.h"
#include "ioexp.h"
#include "config.h"

static bool g_presente;

void matrix_init(void)
{
    ioexp_init();
    g_presente = i2c_bus_responde(MATRIZ_PCF_SEL) && ioexp_presente();
    if (g_presente) {
        matrix_liberar();
    }
}

bool matrix_presente(void)
{
    return g_presente;
}

void matrix_liberar(void)
{
    if (!g_presente) {
        return;
    }
    ioexp_set(IOEXP_BIT_EN_MUX, IOEXP_BIT_EN_MUX);      /* /EN em alto = desabilitado */
}

cont_estado_t matrix_medir_par(uint8_t a, uint8_t b, bool rapido, int32_t *r_ohm)
{
    cont_estado_t est;
    int32_t r = 0;

    if (!g_presente || a >= MATRIZ_CANAIS || b >= MATRIZ_CANAIS || r_ohm == NULL) {
        return CONT_ERRO;
    }
    if (ioexp_set(IOEXP_BIT_EN_MUX, IOEXP_BIT_EN_MUX) != ESP_OK ||
        i2c_bus_escrever8(MATRIZ_PCF_SEL, (uint8_t)((b << 4) | a)) != ESP_OK ||
        ioexp_set(IOEXP_BIT_EN_MUX, 0x00) != ESP_OK) {   /* /EN_A=/EN_B=0 */
        matrix_liberar();
        return CONT_ERRO;
    }
    est = continuity_medir_n(rapido ? 4 : 16, rapido, &r);
    matrix_liberar();
    if (est == CONT_VALOR) {
        r -= MUX_OFFSET_OHM;
        *r_ohm = (r < 0) ? 0 : r;
    }
    return est;
}
