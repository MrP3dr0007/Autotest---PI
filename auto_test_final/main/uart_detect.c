/*
 * uart_detect.c - Estima o baud medindo o MENOR pulso em nivel BAIXO
 *
 * Uma linha UART em repouso fica em ALTO; o menor pulso BAIXO de um quadro de
 * texto tem a duracao de 1 bit (1/baud). Amostramos o GPIO a cada
 * UART_US_POR_AMOSTRA us (valor a CALIBRAR) e convertemos para baud.
 * Limite pratico: ate ~38400 baud; 57600/115200 saem como "no limite".
 * Nao detecta paridade nem formato. Logica 3,3 V SOMENTE.
 */
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "uart_detect.h"
#include "config.h"

static const uint32_t k_bauds[] = { 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200 };

static void pino_entrada(void)
{
    gpio_config_t c;

    c.pin_bit_mask = (1ULL << PIN_UART_RX);
    c.mode = GPIO_MODE_INPUT;
    c.pull_up_en = GPIO_PULLUP_DISABLE;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&c);
}

void uart_detectar(int janela_ms, uart_medida_t *m)
{
    TickType_t fim;
    uint32_t menor = 0xFFFFFFFFu;

    if (m == NULL) {
        return;
    }
    m->pulsos = 0;
    m->menor_us = 0;
    m->baud = 0;
    m->limite = 0;
    pino_entrada();
    fim = xTaskGetTickCount() + (TickType_t)(janela_ms / portTICK_PERIOD_MS);

    while ((int32_t)(fim - xTaskGetTickCount()) > 0) {
        int achou = 0;

        portENTER_CRITICAL();
        for (int i = 0; i < 400 && !achou; i++) {            /* ~4 ms esperando a borda */
            if (gpio_get_level(PIN_UART_RX) == 0) {
                achou = 1;
            } else {
                ets_delay_us(10);
            }
        }
        if (achou) {
            uint32_t corrida = 0;

            for (int k = 0; k < 250; k++) {                  /* ~3 ms de captura */
                if (gpio_get_level(PIN_UART_RX) == 0) {
                    corrida++;
                } else {
                    if (corrida > 0) {
                        m->pulsos++;
                        if (corrida < menor) { menor = corrida; }
                    }
                    corrida = 0;
                }
                ets_delay_us(UART_US_POR_AMOSTRA);
            }
        }
        portEXIT_CRITICAL();
        vTaskDelay(1);
    }

    if (m->pulsos >= 3 && menor != 0xFFFFFFFFu) {
        uint32_t bit_us = menor * UART_US_POR_AMOSTRA;
        uint32_t melhor = 0, dif_melhor = 0xFFFFFFFFu;

        m->menor_us = bit_us;
        if (menor <= 1) {
            m->limite = 1;          /* 1 amostra: resolucao insuficiente */
        }
        for (size_t k = 0; k < sizeof(k_bauds) / sizeof(k_bauds[0]); k++) {
            uint32_t t = 1000000u / k_bauds[k];
            uint32_t d = (t > bit_us) ? (t - bit_us) : (bit_us - t);

            if (d < dif_melhor) { dif_melhor = d; melhor = k_bauds[k]; }
        }
        m->baud = melhor;
    }
}
