/*
 * continuity.c - Continuidade por divisor de tensao (ESP8266_RTOS_SDK v3.4)
 *
 *   GPIO14 --[R1 1k]--(ponta A)   ...circuito sob teste...   (ponta B)--+-- A0
 *                                                                       |
 *                                                          pull-down 10k externo -- GND
 *
 * Vb = Vd * Rpd / (R1 + Rx + Rpd)  =>  Rx = Rpd * (Vd - Vb) / Vb - R1
 * Corrente maxima injetada: 3,3 V / 1 kohm = 3,3 mA (ponta A em curto com GND).
 * Resolucao pratica: algumas dezenas de ohms. Nao serve para medir miliohm.
 * Calibre com resistores conhecidos (0 ohm, 100 ohm, 1 kohm).
 */
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "rom/ets_sys.h"       /* ets_delay_us */
#include "continuity.h"
#include "config.h"

#define AMOSTRAS_ADC 16

static bool g_adc_ok;

static void ponta_a_solta(void)
{
    gpio_config_t c;

    c.pin_bit_mask = (1ULL << PIN_PONTA_A);
    c.mode = GPIO_MODE_INPUT;               /* alta impedancia */
    c.pull_up_en = GPIO_PULLUP_DISABLE;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&c);
}

void continuity_init(void)
{
    adc_config_t a;

    ponta_a_solta();
    a.mode = ADC_READ_TOUT_MODE;
    a.clk_div = 8;
    g_adc_ok = (adc_init(&a) == ESP_OK);
}

static bool ler_adc_mv(int32_t *mv, int n, bool rapido)
{
    uint32_t soma = 0;
    uint16_t v;

    for (int k = 0; k < n; k++) {
        if (adc_read(&v) != ESP_OK) {
            return false;
        }
        soma += v;
        if (rapido) { ets_delay_us(100); } else { vTaskDelay(1); }
    }
    *mv = (int32_t)((soma / (uint32_t)n) * A0_FUNDO_ESCALA_MV / 1023u);
    return true;
}

cont_estado_t continuity_medir_n(int amostras, bool rapido, int32_t *r_ohm)
{
    gpio_config_t c;
    int32_t vb, rx;
    bool ok;

    if (!g_adc_ok || r_ohm == NULL || amostras < 1) {
        return CONT_ERRO;
    }
    c.pin_bit_mask = (1ULL << PIN_PONTA_A);
    c.mode = GPIO_MODE_OUTPUT;
    c.pull_up_en = GPIO_PULLUP_DISABLE;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&c);
    gpio_set_level(PIN_PONTA_A, 1);
    if (rapido) { ets_delay_us(500); } else { vTaskDelay(5 / portTICK_PERIOD_MS + 1); }   /* assenta o sinal */
    ok = ler_adc_mv(&vb, amostras, rapido);
    ponta_a_solta();                                 /* nunca deixa a ponta energizada */
    if (!ok) {
        return CONT_ERRO;
    }
    if (vb < CONT_ABERTO_MV) {
        return CONT_ABERTO;
    }
    if (vb > CONT_VDRIVE_MV) {
        vb = CONT_VDRIVE_MV;
    }
    rx = (int32_t)(((int64_t)CONT_RPD_OHM * (CONT_VDRIVE_MV - vb)) / vb) - CONT_R1_OHM;
    *r_ohm = (rx < 0) ? 0 : rx;
    return CONT_VALOR;
}

cont_estado_t continuity_medir(int32_t *r_ohm)
{
    return continuity_medir_n(AMOSTRAS_ADC, false, r_ohm);
}
