/* board_io.c - Acesso aos GPIOs da placa */
#include <stdint.h>
#include "driver/gpio.h"
#include "board_io.h"
#include "config.h"

static void configurar_pino(int pino, gpio_mode_t modo, bool pullup)
{
    gpio_config_t cfg;

    cfg.pin_bit_mask = (1ULL << pino);
    cfg.mode         = modo;
    cfg.pull_up_en   = pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
}

void board_io_init(void)
{
    configurar_pino(PIN_LED_STATUS, GPIO_MODE_OUTPUT, false);
    board_led_set(false);
    configurar_pino(PIN_BOTAO_OPERADOR, GPIO_MODE_INPUT, true);
#if POTENCIA_INSTALADA
    configurar_pino(PIN_POTENCIA, GPIO_MODE_OUTPUT, false);
#endif
    board_potencia_set(false);                 /* sempre inicia DESLIGADA */
}

void board_io_estado_seguro(void)
{
    board_led_set(false);
    board_potencia_set(false);                 /* corta a carga */
}

void board_led_set(bool aceso)
{
    int nivel = (aceso ? 1 : 0) ^ LED_ATIVO_BAIXO;
    gpio_set_level(PIN_LED_STATUS, nivel);
}

bool board_botao_pressionado(void)
{
    return gpio_get_level(PIN_BOTAO_OPERADOR) == 0;
}

static volatile bool g_potencia;

void board_potencia_set(bool ligada)
{
#if POTENCIA_INSTALADA
    gpio_set_level(PIN_POTENCIA, (ligada == (POTENCIA_ATIVO_ALTO != 0)) ? 1 : 0);
    g_potencia = ligada;
#else
    (void)ligada;
    g_potencia = false;
#endif
}

bool board_potencia_ligada(void)
{
    return g_potencia;
}
