/*
 * result_logger.c - Historico circular em RAM com copia persistente (NVS)
 *
 * Cada registro e gravado na NVS ("r0".."r7") e o indice em "meta". Ao ligar,
 * o historico e recarregado. A copia so vale se o tamanho do registro bater
 * (mudou de versao do firmware = comeca vazio). Nao ha RTC: a data real so
 * existe se o navegador ajustou o relogio antes do teste (campo epoch).
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "result_logger.h"
#include "profile_manager.h"
#include "relogio.h"
#include "config.h"

typedef struct { int32_t total, proximo; uint32_t ultimo_id; uint32_t tam_registro; } meta_t;

static registro_t        g_reg[MAX_HISTORICO];
static int               g_total;          /* quantos registros validos */
static int               g_proximo;        /* proxima posicao de escrita */
static uint32_t          g_ultimo_id;
static SemaphoreHandle_t g_mutex;

static void salvar(int pos)
{
    nvs_handle h;
    char chave[8];
    meta_t m;

    if (nvs_open("historico", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    snprintf(chave, sizeof(chave), "r%d", pos);
    nvs_set_blob(h, chave, &g_reg[pos], sizeof(registro_t));
    m.total = g_total;
    m.proximo = g_proximo;
    m.ultimo_id = g_ultimo_id;
    m.tam_registro = (uint32_t)sizeof(registro_t);
    nvs_set_blob(h, "meta", &m, sizeof(m));
    nvs_commit(h);
    nvs_close(h);
}

static void carregar(void)
{
    nvs_handle h;
    meta_t m;
    size_t tam = sizeof(m);

    if (nvs_open("historico", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    if (nvs_get_blob(h, "meta", &m, &tam) == ESP_OK && tam == sizeof(m) &&
        m.tam_registro == sizeof(registro_t) && m.total >= 0 && m.total <= MAX_HISTORICO &&
        m.proximo >= 0 && m.proximo < MAX_HISTORICO) {
        int validos = 0;

        for (int k = 0; k < MAX_HISTORICO; k++) {
            char chave[8];
            size_t t = sizeof(registro_t);

            snprintf(chave, sizeof(chave), "r%d", k);
            if (nvs_get_blob(h, chave, &g_reg[k], &t) == ESP_OK && t == sizeof(registro_t)) {
                g_reg[k].perfil_id[sizeof(g_reg[k].perfil_id) - 1] = '\0';
                validos++;
            } else {
                memset(&g_reg[k], 0, sizeof(registro_t));
            }
        }
        if (validos >= m.total) {
            g_total = m.total;
            g_proximo = m.proximo;
            g_ultimo_id = m.ultimo_id;
        }
    }
    nvs_close(h);
}

void result_logger_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    g_total = 0;
    g_proximo = 0;
    g_ultimo_id = 0;
    memset(g_reg, 0, sizeof(g_reg));
    carregar();
}

void result_logger_registrar(const sessao_t *s)
{
    const perfil_t *p = perfil_obter(s->perfil);
    registro_t r;
    int pos;

    memset(&r, 0, sizeof(r));
    strncpy(r.perfil_id, p ? p->id : "?", sizeof(r.perfil_id) - 1);
    r.modo = s->modo;
    r.geral = s->geral;
    r.uptime_s = s->fim_ms / 1000;
    r.epoch = s->epoch;
    r.duracao_ms = s->fim_ms - s->inicio_ms;
    r.med_valida = s->med_valida;
    r.v_mv = s->v_mv;
    r.i_ma = s->i_ma;
    for (int i = 0; i < NUM_TESTES; i++) {
        r.testes[i] = s->testes[i].resultado;
    }

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    r.id = ++g_ultimo_id;
    pos = g_proximo;
    g_reg[pos] = r;
    g_proximo = (g_proximo + 1) % MAX_HISTORICO;
    if (g_total < MAX_HISTORICO) {
        g_total++;
    }
    salvar(pos);
    xSemaphoreGive(g_mutex);
}

int result_logger_total(void)
{
    return g_total;
}

uint32_t result_logger_ultimo_id(void)
{
    return g_ultimo_id;
}

bool result_logger_obter(int indice_recente, registro_t *saida)
{
    int pos;

    if (saida == NULL || indice_recente < 0) {
        return false;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    if (indice_recente >= g_total) {
        xSemaphoreGive(g_mutex);
        return false;
    }
    pos = (g_proximo - 1 - indice_recente + 2 * MAX_HISTORICO) % MAX_HISTORICO;
    *saida = g_reg[pos];
    xSemaphoreGive(g_mutex);
    return true;
}

bool result_logger_por_id(uint32_t id, registro_t *saida)
{
    registro_t r;

    for (int i = 0; result_logger_obter(i, &r); i++) {
        if (r.id == id) {
            *saida = r;
            return true;
        }
    }
    return false;
}

bool result_logger_anterior(const char *perfil_id, uint32_t antes_do_id, registro_t *saida)
{
    registro_t r;

    for (int i = 0; result_logger_obter(i, &r); i++) {
        if (r.id < antes_do_id && r.med_valida && strcmp(r.perfil_id, perfil_id) == 0) {
            *saida = r;
            return true;
        }
    }
    return false;
}
