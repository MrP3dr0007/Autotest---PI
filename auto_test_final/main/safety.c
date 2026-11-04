/*
 * safety.c - Estado de seguranca da bancada
 *
 * Etapa 3: o disparo solta os pinos de teste, ABRE a chave de potencia (se
 * POTENCIA_INSTALADA) e impede novos testes ate o operador rearmar. O corte
 * por software leva ~200 ms: a protecao RAPIDA e o limite de corrente da fonte.
 */
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "safety.h"
#include "board_io.h"
#include "measure.h"
#include "matrix.h"
#include "config.h"

static const char *TAG = "SAFETY";
static volatile seguranca_estado_t g_estado = SEG_OK;
static volatile seguranca_motivo_t g_motivo = SEG_MOTIVO_NENHUM;

static void tarefa_monitor(void *arg)
{
    (void)arg;
    for (;;) {
        if (esp_get_free_heap_size() < HEAP_SEGURANCA_BYTES) {
            safety_disparar(SEG_MOTIVO_MEMORIA_BAIXA);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void safety_init(void)
{
    g_estado = SEG_OK;
    g_motivo = SEG_MOTIVO_NENHUM;
    xTaskCreate(tarefa_monitor, "seguranca", 2048, NULL, 6, NULL);
}

bool safety_ok(void)
{
    return g_estado == SEG_OK;
}

void safety_disparar(seguranca_motivo_t motivo)
{
    if (g_estado != SEG_OK) {
        return;                         /* ja desarmada: mantem o 1o motivo */
    }
    g_motivo = motivo;
    g_estado = (motivo == SEG_MOTIVO_PARADA_OPERADOR) ? SEG_PARADA_EMERGENCIA : SEG_FALHA;
    board_io_estado_seguro();
    matrix_liberar();                 /* desconecta todos os pontos de teste */
    ESP_LOGW(TAG, "Seguranca disparada: %s", safety_motivo_texto());
}

bool safety_rearmar(void)
{
    if (esp_get_free_heap_size() < HEAP_SEGURANCA_BYTES || measure_em_risco()) {
        return false;                   /* condicao de risco ainda presente */
    }
    g_motivo = SEG_MOTIVO_NENHUM;
    g_estado = SEG_OK;
    return true;
}

seguranca_estado_t safety_estado(void)
{
    return g_estado;
}

const char *safety_motivo_texto(void)
{
    switch (g_motivo) {
    case SEG_MOTIVO_PARADA_OPERADOR: return "Parada de emergência acionada pelo operador";
    case SEG_MOTIVO_MEMORIA_BAIXA:   return "Memória livre abaixo do limite de segurança";
    case SEG_MOTIVO_SOBRETENSAO:     return "Sobretensão detectada. Saída cortada (sem módulo de potência: desligue a fonte)";
    case SEG_MOTIVO_SOBRECORRENTE:   return "Sobrecorrente detectada. Saída cortada (sem módulo de potência: desligue a fonte)";
    default:                         return "";
    }
}
