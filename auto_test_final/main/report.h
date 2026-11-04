/* report.h - Relatorio imprimivel por diagnostico (HTML) */
#ifndef REPORT_H
#define REPORT_H
#include "esp_http_server.h"
#include "test_manager.h"

/* id = 0 ou id da ultima sessao concluida: relatorio COMPLETO (com detalhes);
 * outro id: resumo do historico (sem detalhes por teste). */
esp_err_t relatorio_enviar(httpd_req_t *req, uint32_t id, const sessao_t *ultima);

#endif
