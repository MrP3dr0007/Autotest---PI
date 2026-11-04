/*
 * report.c - Relatorio por diagnostico: perfil, condicoes, resultados, valores
 * de referencia (limites e leitura anterior do mesmo perfil).
 * Enviado em partes (chunked) para nao gastar RAM. Imprima pelo navegador
 * (botao Imprimir / Salvar como PDF). So mostra leituras reais registradas.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "report.h"
#include "result_logger.h"
#include "profile_manager.h"
#include "config.h"

static void out(httpd_req_t *req, const char *fmt, ...)
{
    static char t[512];          /* o servidor HTTP atende uma requisicao por vez */
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(t, sizeof(t), fmt, ap);
    va_end(ap);
    if (n > 0) {
        httpd_resp_send_chunk(req, t, (n < (int)sizeof(t)) ? n : (int)sizeof(t) - 1);
    }
}

static const char *perfil_nome(const char *id)
{
    for (int i = 0; i < perfil_total(); i++) {
        const perfil_t *p = perfil_obter(i);

        if (strcmp(p->id, id) == 0) { return p->nome; }
    }
    return id;
}

static void cabecalho(httpd_req_t *req, uint32_t id)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    static const char k_topo[] =
        "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'><style>"
        "body{font-family:Arial,sans-serif;max-width:760px;margin:16px auto;padding:0 12px;color:#111}"
        "table{border-collapse:collapse;width:100%}td,th{border:1px solid #999;padding:5px;font-size:13px;text-align:left}"
        "h1{font-size:20px}h2{font-size:15px;margin-top:18px}.APROVADO{color:#067d2b;font-weight:700}"
        ".REPROVADO,.SEGURANCA{color:#b00020;font-weight:700}.INCONCLUSIVO{color:#8a6100;font-weight:700}"
        ".n{font-size:12px;color:#444}@media print{button{display:none}}</style>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send_chunk(req, k_topo, (ssize_t)strlen(k_topo));
    out(req, "<title>Relatório #%u</title></head><body>", (unsigned)id);
    out(req, "<button onclick='window.print()'>Imprimir / Salvar PDF</button>"
             "<h1>AUTO TEST — Relatório de diagnóstico #%u</h1>", (unsigned)id);
}

static void data_html(httpd_req_t *req, uint32_t epoch)
{
    if (epoch == 0) {
        out(req, "<p>Data: não registrada (o relógio não foi ajustado pelo navegador antes do teste).</p>");
    } else {
        out(req, "<p>Data: <span id='d' data-ep='%u'></span></p>"
                 "<script>var e=document.getElementById('d');"
                 "e.textContent=new Date(e.dataset.ep*1000).toLocaleString('pt-BR');</script>", (unsigned)epoch);
    }
}

static const char *classe(resultado_t r)
{
    return (r == RES_INTERROMPIDO) ? "SEGURANCA" : resultado_codigo(r);
}

static void comparacao(httpd_req_t *req, const char *perfil_id, uint32_t id, bool med_valida,
                       int32_t v_mv, int32_t i_ma)
{
    registro_t ant;

    out(req, "<h2>Comparação com referência</h2>");
    if (!med_valida) {
        out(req, "<p class='n'>Não houve leitura real de tensão/corrente neste diagnóstico.</p>");
        return;
    }
    if (result_logger_anterior(perfil_id, id, &ant)) {
        out(req, "<p>Diagnóstico anterior do mesmo perfil (#%u): %d,%03d V e %d mA. "
                 "Variação agora: %+d mV e %+d mA.</p>",
            (unsigned)ant.id, (int)(ant.v_mv / 1000), (int)(ant.v_mv % 1000), (int)ant.i_ma,
            (int)(v_mv - ant.v_mv), (int)(i_ma - ant.i_ma));
    } else {
        out(req, "<p class='n'>Não há diagnóstico anterior do mesmo perfil com leitura real para comparar.</p>");
    }
}

esp_err_t relatorio_enviar(httpd_req_t *req, uint32_t id, const sessao_t *ultima)
{
    registro_t r;
    bool completo = false;
    uint32_t ultimo = result_logger_ultimo_id();

    if (id == 0) { id = ultimo; }
    if (id == 0 || !result_logger_por_id(id, &r)) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_send(req, "Relatório não encontrado (histórico guarda os últimos diagnósticos).", -1);
    }
    completo = (id == ultimo && ultima != NULL && ultima->estado == SESSAO_CONCLUIDA);

    cabecalho(req, id);
    out(req, "<table><tr><th>Perfil</th><td>%s (%s)</td></tr>"
             "<tr><th>Modo</th><td>%s</td></tr>"
             "<tr><th>Resultado geral</th><td class='%s'>%s</td></tr>"
             "<tr><th>Duração</th><td>%u,%01u s</td></tr>"
             "<tr><th>Firmware</th><td>%s</td></tr></table>",
        perfil_nome(r.perfil_id), r.perfil_id, modo_texto(r.modo), classe(r.geral), resultado_texto(r.geral),
        (unsigned)(r.duracao_ms / 1000), (unsigned)((r.duracao_ms % 1000) / 100), FW_VERSAO);
    data_html(req, r.epoch);

    out(req, "<h2>Condições do teste</h2>");
    if (completo) {
        const limites_t *l = &ultima->lim;

        out(req, "<table><tr><th>Limite</th><th>Valor usado</th></tr>"
                 "<tr><td>Tensão</td><td>%d,%03d a %d,%03d V (0 = não definido)</td></tr>"
                 "<tr><td>Corrente</td><td>%d a %d mA</td></tr>"
                 "<tr><td>Resistência (continuidade/varredura)</td><td>%d a %d ohm (máx. 0 = sem máximo)</td></tr></table>",
            (int)(l->vmin_mv / 1000), (int)(l->vmin_mv % 1000), (int)(l->vmax_mv / 1000), (int)(l->vmax_mv % 1000),
            (int)l->imin_ma, (int)l->imax_ma, (int)l->rmin_ohm, (int)l->rmax_ohm);
    } else {
        out(req, "<p class='n'>Os limites e detalhes por teste só ficam disponíveis para o diagnóstico mais recente "
                 "da sessão atual (a memória guarda apenas o resumo dos anteriores).</p>");
    }
    if (r.med_valida) {
        out(req, "<p>Última leitura real (INA219): %d,%03d V e %d mA. Erro de medição não caracterizado: calibre com multímetro.</p>",
            (int)(r.v_mv / 1000), (int)(r.v_mv % 1000), (int)r.i_ma);
    } else {
        out(req, "<p class='n'>Nenhuma leitura de tensão/corrente foi obtida (nada foi simulado).</p>");
    }

    out(req, "<h2>Resultados</h2><table><tr><th>Teste</th><th>Resultado</th>%s</tr>", completo ? "<th>Detalhe</th>" : "");
    for (int i = 0; i < NUM_TESTES; i++) {
        resultado_t t = completo ? ultima->testes[i].resultado : r.testes[i];

        if (t == RES_NAO_APLICAVEL || t == RES_PENDENTE) { continue; }
        if (completo) {
            out(req, "<tr><td>%s</td><td class='%s'>%s</td><td>%s</td></tr>", test_manager_nome(i), classe(t),
                resultado_texto(t), ultima->testes[i].detalhe);
        } else {
            out(req, "<tr><td>%s</td><td class='%s'>%s</td></tr>", test_manager_nome(i), classe(t), resultado_texto(t));
        }
    }
    out(req, "</table>");
    comparacao(req, r.perfil_id, id, r.med_valida, r.v_mv, r.i_ma);
    out(req, "<p class='n'>APROVADO = dentro dos limites definidos. Perfis não validados só indicam conformidade com os "
             "limites informados pelo operador, não garantem o funcionamento da placa. Documento gerado pela bancada Auto Test.</p>"
             "</body></html>");
    return httpd_resp_send_chunk(req, NULL, 0);
}
