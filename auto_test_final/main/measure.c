/*
 * measure.c - Medicao com INA219 (I2C) e monitor continuo de limites
 *
 * Tensao: registro 0x02 (bits 15..3, 4 mV/LSB) = tensao em VIN- (lado da carga).
 * Corrente: registro 0x01 (shunt, 10 uV/LSB, com sinal) dividido pelo shunt.
 *   corrente_mA = shunt_uV / shunt_mOhm   (nao usa o registro de calibracao)
 * Configuracao padrao 0x399F: 32 V, +-320 mV, 12 bits, continuo.
 *
 * ATENCAO: nesta etapa a bancada NAO tem chave de potencia. Quando um limite
 * e violado o firmware desarma a seguranca e avisa, mas quem corta a energia
 * e o OPERADOR (desligando a fonte). O corte automatico e a Etapa 3.
 */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "measure.h"
#include "i2c_bus.h"
#include "safety.h"
#include "config.h"

static const char *TAG = "MEDIDA";

#define REG_CONFIG   0x00
#define REG_SHUNT    0x01
#define REG_BUS      0x02
#define CONFIG_PADRAO 0x399F

static SemaphoreHandle_t g_mutex;
static medida_t  g_ultima;
static limites_t g_lim;
static int       g_falhas;
static int       g_cont_v, g_cont_i;
static volatile bool g_risco;

/* ------------------------------ Leitura -------------------------------- */

static bool ler_bruto(int32_t *tensao_mv, int32_t *corrente_ma)
{
    uint16_t bus, shunt;
    int32_t  v, i;

    if (i2c_bus_ler16(INA219_ENDERECO, REG_BUS, &bus) != ESP_OK ||
        i2c_bus_ler16(INA219_ENDERECO, REG_SHUNT, &shunt) != ESP_OK) {
        return false;
    }
    if (bus & 0x0001) {
        return false;                 /* OVF: conversao saturada */
    }
    v = (int32_t)(bus >> 3) * 4;                       /* mV */
    i = ((int32_t)(int16_t)shunt * 10) / INA219_SHUNT_MOHM;   /* uV / mOhm = mA */
    if (i < 0) {
        i = -i;
    }
    v = v * CAL_TENSAO_GANHO / 1000 + CAL_TENSAO_OFFSET_MV;
    i = i * CAL_CORRENTE_GANHO / 1000 + CAL_CORRENTE_OFFSET_MA;
    if (v < 0) { v = 0; }
    if (i < 0) { i = 0; }
    *tensao_mv = v;
    *corrente_ma = i;
    return true;
}

bool measure_media(int n, medida_t *m)
{
    int32_t sv = 0, si = 0, v, i;

    if (n < 1 || m == NULL) {
        return false;
    }
    for (int k = 0; k < n; k++) {
        if (!ler_bruto(&v, &i)) {
            m->disponivel = false;
            return false;
        }
        sv += v;
        si += i;
        vTaskDelay(5 / portTICK_PERIOD_MS + 1);
    }
    m->disponivel = true;
    m->tensao_mv = sv / n;
    m->corrente_ma = si / n;
    return true;
}

bool measure_presente(void)
{
    medida_t m;

    measure_ultima(&m);
    return m.disponivel;
}

void measure_ultima(medida_t *m)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    *m = g_ultima;
    xSemaphoreGive(g_mutex);
}

bool measure_em_risco(void)
{
    return g_risco;
}

/* ------------------------------- Limites ------------------------------- */

void limites_obter(limites_t *l)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    *l = g_lim;
    xSemaphoreGive(g_mutex);
}

const char *limites_definir(const limites_t *l)
{
    nvs_handle h;

    if (l == NULL) { return "Parâmetros ausentes."; }
    if (l->vmin_mv < 0 || l->imin_ma < 0 || l->vmax_mv < 0 || l->imax_ma < 0) {
        return "Valores negativos não são aceitos.";
    }
    if (l->rmin_ohm < 0 || l->rmax_ohm < 0) { return "Resistência negativa não é aceita."; }
    if (l->rmax_ohm != 0 && l->rmin_ohm > l->rmax_ohm) { return "Resistência mínima maior que a máxima."; }
    if (l->vmax_mv > TETO_TENSAO_MV) { return "Tensão máxima acima do teto do hardware."; }
    if (l->imax_ma > TETO_CORRENTE_MA) { return "Corrente máxima acima do teto do hardware."; }
    if (l->vmax_mv != 0 && l->vmin_mv > l->vmax_mv) { return "Tensão mínima maior que a máxima."; }
    if (l->imax_ma != 0 && l->imin_ma > l->imax_ma) { return "Corrente mínima maior que a máxima."; }

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_lim = *l;
    g_cont_v = g_cont_i = 0;
    xSemaphoreGive(g_mutex);

    if (nvs_open("autotest", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "limites", l, sizeof(*l));
        nvs_commit(h);
        nvs_close(h);
    }
    return NULL;
}

static void carregar_limites(void)
{
    nvs_handle h;
    size_t tam = sizeof(limites_t);
    limites_t l;

    memset(&g_lim, 0, sizeof(g_lim));
    if (nvs_open("autotest", NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_blob(h, "limites", &l, &tam) == ESP_OK && tam == sizeof(l)) {
            /* revalida: a flash pode ter valores de outra versao */
            if (l.vmax_mv >= 0 && l.vmax_mv <= TETO_TENSAO_MV &&
                l.imax_ma >= 0 && l.imax_ma <= TETO_CORRENTE_MA &&
                l.vmin_mv >= 0 && l.imin_ma >= 0 &&
                l.rmin_ohm >= 0 && l.rmax_ohm >= 0) {
                g_lim = l;
            }
        }
        nvs_close(h);
    }
}

/* ------------------------------- Monitor ------------------------------- */

static void tarefa_monitor(void *arg)
{
    int32_t v, i;
    limites_t lim;

    (void)arg;
    for (;;) {
        bool ok = ler_bruto(&v, &i);

        xSemaphoreTake(g_mutex, portMAX_DELAY);
        if (ok) {
            g_falhas = 0;
            g_ultima.disponivel = true;
            g_ultima.tensao_mv = v;
            g_ultima.corrente_ma = i;
        } else if (++g_falhas >= MEDIDA_FALHAS_MAX) {
            g_ultima.disponivel = false;
        }
        lim = g_lim;
        xSemaphoreGive(g_mutex);

        if (ok) {
            bool sobre_v = (v > TETO_TENSAO_MV) || (lim.vmax_mv > 0 && v > lim.vmax_mv);
            bool sobre_i = (i > TETO_CORRENTE_MA) || (lim.imax_ma > 0 && i > lim.imax_ma);

            g_cont_v = sobre_v ? g_cont_v + 1 : 0;
            g_cont_i = sobre_i ? g_cont_i + 1 : 0;
            g_risco = (g_cont_v > 0) || (g_cont_i > 0);
            if (g_cont_v >= MEDIDA_CONFIRMACOES) {
                safety_disparar(SEG_MOTIVO_SOBRETENSAO);
            } else if (g_cont_i >= MEDIDA_CONFIRMACOES) {
                safety_disparar(SEG_MOTIVO_SOBRECORRENTE);
            }
        }
        vTaskDelay(MEDIDA_PERIODO_MS / portTICK_PERIOD_MS);
    }
}

void measure_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    memset(&g_ultima, 0, sizeof(g_ultima));
    carregar_limites();

    /* Configura o INA219; se nao responder, o monitor continua tentando. */
    if (i2c_bus_escrever16(INA219_ENDERECO, REG_CONFIG, CONFIG_PADRAO) != ESP_OK) {
        ESP_LOGW(TAG, "INA219 nao respondeu em 0x%02X (ligado? SDA/SCL trocados?)",
                 INA219_ENDERECO);
    }
    xTaskCreate(tarefa_monitor, "medida", 2560, NULL, 5, NULL);
}
