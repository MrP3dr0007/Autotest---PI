/*
 * web_interface.c - Painel web e API HTTP/JSON
 *
 * Rotas:
 *   GET  /                 pagina do operador
 *   GET  /api/status       estado da bancada e da sessao
 *   GET  /api/perfis       perfis disponiveis
 *   GET  /api/historico    ultimos diagnosticos
 *   GET  /export.csv       historico em CSV
 *   POST /api/iniciar?perfil=N&modo=assistido|automatico
 *   POST /api/confirmar?v=1|0
 *   POST /api/parada       parada de emergencia
 *   POST /api/rearmar
 *   POST /api/limites?vmin=&vmax=&imin=&imax=&rmin=&rmax=  (mV, mA, ohm; inteiros)
 *   POST /api/saida?v=1|0   liga/desliga a saida de potencia
 *   GET  /relatorio?id=N    relatorio imprimivel (N omitido = ultimo)
 *   POST /api/hora?t=epoch  ajusta o relogio (o painel envia sozinho)
 *
 * Limitacoes: sem autenticacao, sem HTTPS. Os textos enviados em JSON nao
 * podem conter aspas duplas nem barra invertida.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "web_interface.h"
#include "pagina.h"
#include "test_manager.h"
#include "profile_manager.h"
#include "result_logger.h"
#include "safety.h"
#include "measure.h"
#include "power.h"
#include "matrix.h"
#include "report.h"
#include "relogio.h"
#include "result_logger.h"
#include "config.h"

static const char *TAG = "WEB";

/* O servidor HTTP atende uma requisicao por vez: um buffer compartilhado basta. */
static char g_resp[4096];
static sessao_t g_sessao;     /* copia compartilhada (status e relatorio) */

typedef struct { char *buf; size_t tam; size_t n; } json_t;

static void jadd(json_t *j, const char *fmt, ...)
{
    va_list ap;

    if (j->n >= j->tam - 1) {
        return;
    }
    va_start(ap, fmt);
    int r = vsnprintf(j->buf + j->n, j->tam - j->n, fmt, ap);
    va_end(ap);
    if (r > 0) {
        j->n += (size_t)r;
        if (j->n > j->tam - 1) {
            j->n = j->tam - 1;
        }
    }
}

static bool parametro(httpd_req_t *req, const char *chave, char *saida, size_t tam)
{
    char consulta[96];

    if (httpd_req_get_url_query_str(req, consulta, sizeof(consulta)) != ESP_OK) {
        return false;
    }
    return httpd_query_key_value(consulta, chave, saida, tam) == ESP_OK;
}

static esp_err_t responder_json(httpd_req_t *req, const char *corpo, size_t n)
{
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, corpo, (ssize_t)n);
}

static esp_err_t responder_ok(httpd_req_t *req, bool ok, const char *msg)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "{\"ok\":%s,\"msg\":\"%s\"}", ok ? "true" : "false", msg);

    if (!ok) {
        httpd_resp_set_status(req, "400 Bad Request");
    }
    return responder_json(req, buf, (size_t)n);
}

/* --------------------------------- Rotas ------------------------------ */

static esp_err_t rota_pagina(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGINA_HTML, (ssize_t)strlen(PAGINA_HTML));
}

static const char *matriz_presente_txt(void)
{
    return matrix_presente() ? "true" : "false";
}

static esp_err_t rota_status(httpd_req_t *req)
{
    char *buf = g_resp;
    sessao_t *sp = &g_sessao;
    json_t j = { buf, sizeof(g_resp), 0 };
    uint32_t decorrido;

    test_manager_copiar(sp);
    if (sp->estado == SESSAO_EXECUTANDO)      decorrido = test_manager_agora_ms() - sp->inicio_ms;
    else if (sp->estado == SESSAO_CONCLUIDA)  decorrido = sp->fim_ms - sp->inicio_ms;
    else                                    decorrido = 0;

    jadd(&j, "{\"fw\":\"%s\",\"seguranca\":{\"ok\":%s,\"motivo\":\"%s\"},",
         FW_VERSAO, safety_ok() ? "true" : "false", safety_motivo_texto());
    {
        medida_t m;
        limites_t l;

        measure_ultima(&m);
        limites_obter(&l);
        jadd(&j, "\"alimentacao\":{\"instalada\":%s,\"ligada\":%s,\"texto\":\"%s\"},",
             power_instalada() ? "true" : "false", power_ligada() ? "true" : "false",
             power_instalada() ? "Chave de potência instalada. O limite de corrente da fonte é a proteção rápida."
                               : "Módulo de potência não instalado: ligue e desligue a fonte manualmente.");
        if (m.disponivel) {
            jadd(&j, "\"medicoes\":{\"disponivel\":true,\"tensao_mv\":%d,\"corrente_ma\":%d,"
                     "\"texto\":\"Leitura real do INA219. Calibre com multímetro: erro não caracterizado.\"},",
                 (int)m.tensao_mv, (int)m.corrente_ma);
        } else {
            jadd(&j, "\"medicoes\":{\"disponivel\":false,\"tensao_mv\":null,\"corrente_ma\":null,"
                     "\"texto\":\"INA219 não detectado. Nenhum valor é simulado.\"},");
        }
        jadd(&j, "\"matriz\":%s,\"limites\":{\"vmin\":%d,\"vmax\":%d,\"imin\":%d,\"imax\":%d,\"rmin\":%d,\"rmax\":%d,\"tetoV\":%d,\"tetoI\":%d},",
             matriz_presente_txt(), (int)l.vmin_mv, (int)l.vmax_mv, (int)l.imin_ma, (int)l.imax_ma,
             (int)l.rmin_ohm, (int)l.rmax_ohm, TETO_TENSAO_MV, TETO_CORRENTE_MA);
    }
    jadd(&j, "\"sessao\":{\"estado\":\"%s\",\"modo\":\"%s\",\"perfil\":%d,\"geral\":\"%s\","
             "\"geralCod\":\"%s\",\"decorridoMs\":%u,\"aguardando\":%s,\"pergunta\":\"%s\"},",
         sp->estado == SESSAO_EXECUTANDO ? "EXECUTANDO" : (sp->estado == SESSAO_CONCLUIDA ? "CONCLUÍDA" : "OCIOSA"),
         modo_texto(sp->modo), sp->perfil, resultado_texto(sp->geral), resultado_codigo(sp->geral),
         (unsigned)decorrido, sp->aguardando ? "true" : "false", sp->pergunta);
    jadd(&j, "\"testes\":[");
    int escritos = 0;
    for (int i = 0; i < NUM_TESTES; i++) {
        if (sp->testes[i].resultado == RES_NAO_APLICAVEL) {
            continue;                      /* so lista os testes do perfil */
        }
        jadd(&j, "%s{\"nome\":\"%s\",\"cod\":\"%s\",\"txt\":\"%s\",\"detalhe\":\"%s\"}",
             escritos++ ? "," : "", sp->testes[i].nome, resultado_codigo(sp->testes[i].resultado),
             resultado_texto(sp->testes[i].resultado), sp->testes[i].detalhe);
    }
    jadd(&j, "]}");
    return responder_json(req, buf, j.n);
}

static esp_err_t rota_perfis(httpd_req_t *req)
{
    char *buf = g_resp;
    json_t j = { buf, sizeof(g_resp), 0 };

    jadd(&j, "[");
    for (int i = 0; i < perfil_total(); i++) {
        const perfil_t *p = perfil_obter(i);
        jadd(&j, "%s{\"id\":\"%s\",\"nome\":\"%s\",\"descricao\":\"%s\",\"alertas\":\"%s\","
                 "\"validado\":%s,\"automatico\":%s,"
                 "\"vmin\":%d,\"vmax\":%d,\"imin\":%d,\"imax\":%d,\"rmin\":%d,\"rmax\":%d}",
             i ? "," : "", p->id, p->nome, p->descricao, p->alertas,
             p->validado ? "true" : "false",
             (p->validado && p->testes_automatico) ? "true" : "false",
             (int)p->limites_padrao.vmin_mv, (int)p->limites_padrao.vmax_mv,
             (int)p->limites_padrao.imin_ma, (int)p->limites_padrao.imax_ma,
             (int)p->limites_padrao.rmin_ohm, (int)p->limites_padrao.rmax_ohm);
    }
    jadd(&j, "]");
    return responder_json(req, buf, j.n);
}

static esp_err_t rota_historico(httpd_req_t *req)
{
    char *buf = g_resp;
    json_t j = { buf, sizeof(g_resp), 0 };
    registro_t r;

    jadd(&j, "[");
    for (int i = 0; result_logger_obter(i, &r); i++) {
        jadd(&j, "%s{\"id\":%u,\"uptime\":%u,\"perfil\":\"%s\",\"modo\":\"%s\","
                 "\"geral\":\"%s\",\"cod\":\"%s\",\"duracaoMs\":%u,\"epoch\":%u}",
             i ? "," : "", (unsigned)r.id, (unsigned)r.epoch, (unsigned)r.uptime_s, r.perfil_id, modo_texto(r.modo),
             resultado_texto(r.geral), resultado_codigo(r.geral), (unsigned)r.duracao_ms, (unsigned)r.epoch);
    }
    jadd(&j, "]");
    return responder_json(req, buf, j.n);
}

static esp_err_t rota_csv(httpd_req_t *req)
{
    char linha[640];
    registro_t r;
    int n;

    httpd_resp_set_type(req, "text/csv; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=auto_test_historico.csv");
    n = snprintf(linha, sizeof(linha),
                 "id,epoch_s,uptime_s,perfil,modo,resultado_geral,sistema,led,botao,i2c,medidor,tensao,corrente,continuidade,pontos,varredura,i2c_reg,spi_id,uart,oled,"
                 "tensao_mV,corrente_mA,duracao_ms\r\n");
    httpd_resp_send_chunk(req, linha, n);
    for (int i = 0; result_logger_obter(i, &r); i++) {
        char vm[16] = "", im[16] = "";    /* vazio = sem leitura real */

        if (r.med_valida) {
            snprintf(vm, sizeof(vm), "%d", (int)r.v_mv);
            snprintf(im, sizeof(im), "%d", (int)r.i_ma);
        }
        n = snprintf(linha, sizeof(linha), "%u,%u,%u,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%u\r\n",
                     (unsigned)r.id, (unsigned)r.epoch, (unsigned)r.uptime_s, r.perfil_id, modo_texto(r.modo),
                     resultado_texto(r.geral),
                     resultado_texto(r.testes[0]), resultado_texto(r.testes[1]),
                     resultado_texto(r.testes[2]), resultado_texto(r.testes[3]),
                     resultado_texto(r.testes[4]), resultado_texto(r.testes[5]),
                     resultado_texto(r.testes[6]), resultado_texto(r.testes[7]), resultado_texto(r.testes[8]),
                     resultado_texto(r.testes[9]), resultado_texto(r.testes[10]),
                     resultado_texto(r.testes[11]), resultado_texto(r.testes[12]),
                     resultado_texto(r.testes[13]), vm, im, (unsigned)r.duracao_ms);
        httpd_resp_send_chunk(req, linha, n);
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}

static bool parametro_int(httpd_req_t *req, const char *chave, int32_t *saida)
{
    char t[12];
    char *fim;
    long v;

    if (!parametro(req, chave, t, sizeof(t)) || t[0] == '\0') {
        return false;
    }
    v = strtol(t, &fim, 10);
    if (*fim != '\0' || v < 0 || v > 100000) {
        return false;
    }
    *saida = (int32_t)v;
    return true;
}

static esp_err_t rota_saida(httpd_req_t *req)
{
    char v[4];
    const char *erro;

    if (!parametro(req, "v", v, sizeof(v))) {
        return responder_ok(req, false, "Parâmetro ausente.");
    }
    if (v[0] == '1') {
        erro = power_ligar();
        return responder_ok(req, erro == NULL, erro ? erro : "Saída ligada.");
    }
    power_desligar();
    return responder_ok(req, true, "Saída desligada.");
}

static esp_err_t rota_relatorio(httpd_req_t *req)
{
    char v[12];
    uint32_t id = 0;

    if (parametro(req, "id", v, sizeof(v))) {
        id = (uint32_t)strtoul(v, NULL, 10);
    }
    test_manager_copiar(&g_sessao);
    return relatorio_enviar(req, id, &g_sessao);
}

static esp_err_t rota_hora(httpd_req_t *req)
{
    char v[16];
    char *fim;
    unsigned long t;

    if (!parametro(req, "t", v, sizeof(v)) || v[0] == '\0') {
        return responder_ok(req, false, "Parâmetro ausente.");
    }
    t = strtoul(v, &fim, 10);
    if (*fim != '\0' || !relogio_definir((uint32_t)t)) {
        return responder_ok(req, false, "Horário inválido.");
    }
    return responder_ok(req, true, "Relógio ajustado.");
}

static esp_err_t rota_limites(httpd_req_t *req)
{
    limites_t l;
    const char *erro;

    if (!parametro_int(req, "vmin", &l.vmin_mv) || !parametro_int(req, "vmax", &l.vmax_mv) ||
        !parametro_int(req, "imin", &l.imin_ma) || !parametro_int(req, "imax", &l.imax_ma) ||
        !parametro_int(req, "rmin", &l.rmin_ohm) || !parametro_int(req, "rmax", &l.rmax_ohm)) {
        return responder_ok(req, false, "Parâmetros ausentes ou inválidos.");
    }
    erro = limites_definir(&l);
    return responder_ok(req, erro == NULL, erro ? erro : "Limites aplicados e salvos.");
}

static esp_err_t rota_iniciar(httpd_req_t *req)
{
    char sp[8], sm[16];
    int perfil;
    modo_t modo;
    tm_erro_t e;

    if (!parametro(req, "perfil", sp, sizeof(sp)) || !parametro(req, "modo", sm, sizeof(sm))) {
        return responder_ok(req, false, "Parâmetros ausentes.");
    }
    perfil = atoi(sp);
    if (strcmp(sm, "assistido") == 0)        modo = MODO_ASSISTIDO;
    else if (strcmp(sm, "automatico") == 0)  modo = MODO_AUTOMATICO;
    else return responder_ok(req, false, "Modo inválido.");

    e = test_manager_iniciar(perfil, modo);
    return responder_ok(req, e == TM_OK, tm_erro_texto(e));
}

static esp_err_t rota_confirmar(httpd_req_t *req)
{
    char v[4];

    if (!parametro(req, "v", v, sizeof(v))) {
        return responder_ok(req, false, "Parâmetro ausente.");
    }
    test_manager_confirmar(v[0] == '1');
    return responder_ok(req, true, "ok");
}

static esp_err_t rota_parada(httpd_req_t *req)
{
    safety_disparar(SEG_MOTIVO_PARADA_OPERADOR);
    return responder_ok(req, true, "Parada de emergência acionada.");
}

static esp_err_t rota_rearmar(httpd_req_t *req)
{
    bool ok = test_manager_rearmar();
    return responder_ok(req, ok, ok ? "Bancada rearmada." : "Não foi possível rearmar agora.");
}

void web_interface_iniciar(void)
{
    httpd_handle_t servidor = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t rotas[] = {
        { .uri = "/",              .method = HTTP_GET,  .handler = rota_pagina },
        { .uri = "/api/status",    .method = HTTP_GET,  .handler = rota_status },
        { .uri = "/api/perfis",    .method = HTTP_GET,  .handler = rota_perfis },
        { .uri = "/api/historico", .method = HTTP_GET,  .handler = rota_historico },
        { .uri = "/export.csv",    .method = HTTP_GET,  .handler = rota_csv },
        { .uri = "/api/iniciar",   .method = HTTP_POST, .handler = rota_iniciar },
        { .uri = "/api/confirmar", .method = HTTP_POST, .handler = rota_confirmar },
        { .uri = "/api/parada",    .method = HTTP_POST, .handler = rota_parada },
        { .uri = "/api/rearmar",   .method = HTTP_POST, .handler = rota_rearmar },
        { .uri = "/api/limites",   .method = HTTP_POST, .handler = rota_limites },
        { .uri = "/api/saida",     .method = HTTP_POST, .handler = rota_saida },
        { .uri = "/relatorio",     .method = HTTP_GET,  .handler = rota_relatorio },
        { .uri = "/api/hora",      .method = HTTP_POST, .handler = rota_hora },
    };

    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;

    if (httpd_start(&servidor, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar o servidor web.");
        return;
    }
    for (size_t i = 0; i < sizeof(rotas) / sizeof(rotas[0]); i++) {
        httpd_register_uri_handler(servidor, &rotas[i]);
    }
    ESP_LOGI(TAG, "Painel em http://192.168.4.1/");
}
