/*
 * relogio.c - Sem RTC, a data real vem do navegador do operador (o painel
 * envia o horario ao abrir). Ate la, os registros ficam com data 0 ("sem data").
 * A precisao e a do relogio do celular/PC; deriva do ESP8266 entre ajustes.
 */
#include "relogio.h"
#include "test_manager.h"

static uint32_t g_base;          /* epoch correspondente a uptime = 0 */

bool relogio_definir(uint32_t epoch_s)
{
    uint32_t up = test_manager_agora_ms() / 1000;

    if (epoch_s < 1700000000u || epoch_s > 4100000000u || epoch_s < up) {
        return false;
    }
    g_base = epoch_s - up;
    return true;
}

uint32_t relogio_agora(void)
{
    if (g_base == 0) {
        return 0;
    }
    return g_base + test_manager_agora_ms() / 1000;
}
