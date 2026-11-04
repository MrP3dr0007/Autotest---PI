/*
 * test_manager.c - Maquina de estados e execucao dos testes (Etapa 2)
 *
 * Testes desta etapa:
 *   SISTEMA  : memoria livre (automatico)
 *   LED      : LED pisca 3 vezes; o OPERADOR informa se viu (validacao visual)
 *   BOTAO    : detecta o botao FLASH (GPIO0) dentro do prazo
 *   I2C      : varre 0x08..0x77 e compara com os enderecos esperados do perfil
 *   MEDIDOR  : o INA219 responde?
 *   TENSAO   : tensao medida x limites ativos (ajustaveis no painel)
 *   CORRENTE : corrente medida x limites ativos
 *   I2C_REG  : registrador de ID de dispositivo I2C conhecido (perfil)
 *   SPI_ID   : ID JEDEC por SPI em software
 *   UART     : baud por software (EXPERIMENTAL)
 *   OLED     : tela SSD1306, validacao VISUAL pelo operador
 *   PONTOS   : pontos nomeados do adaptador x faixa esperada (matriz)
 *   VARREDURA: varre os 16 x 16 pares e conta os que conduzem (matriz)
 *   CONTINUIDADE : resistencia entre as pontas A e B, SO com a placa desenergizada
 * Os valores vem do INA219. Se ele nao responder nada e simulado: o teste
 * fica INCONCLUSIVO. Veredito de TENSAO/CORRENTE = "dentro dos limites que o
 * operador definiu"; so vale como aprovacao da placa se os limites vierem de
 * um perfil validado.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "test_manager.h"
#include "profile_manager.h"
#include "result_logger.h"
#include "safety.h"
#include "board_io.h"
#include "i2c_bus.h"
#include "measure.h"
#include "continuity.h"
#include "matrix.h"
#include "spi_soft.h"
#include "uart_detect.h"
#include "oled.h"
#include "ioexp.h"
#include "relogio.h"
#include "power.h"
#include "config.h"

static const char *TAG = "TESTES";

typedef enum { CONF_SIM, CONF_NAO, CONF_TEMPO, CONF_ABORTADO } conf_t;

static sessao_t          g_s;
static SemaphoreHandle_t g_mutex;
static volatile int      g_resposta;      /* 0 = nenhuma, 1 = sim, 2 = nao */

static const char *k_nomes[NUM_TESTES] = {
    "Verificação do sistema",
    "LED de status (visual)",
    "Botão do operador (FLASH)",
    "Varredura I²C",
    "Medidor INA219",
    "Tensão da fonte",
    "Corrente da carga",
    "Continuidade (desenergizada)",
    "Pontos do adaptador",
    "Varredura de condução A × B",
    "Registradores I²C conhecidos",
    "ID SPI (flash)",
    "Baud UART (detector)",
    "Tela OLED (visual)"
};

uint32_t test_manager_agora_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ------------------------------ Utilitarios --------------------------- */

static void definir(int i, resultado_t r, const char *fmt, ...)
{
    va_list ap;

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_s.testes[i].resultado = r;
    va_start(ap, fmt);
    vsnprintf(g_s.testes[i].detalhe, sizeof(g_s.testes[i].detalhe), fmt, ap);
    va_end(ap);
    xSemaphoreGive(g_mutex);
}

/* Espera 'ms' em passos curtos; devolve false se a seguranca disparou. */
static bool pausa_segura(uint32_t ms)
{
    while (ms > 0) {
        uint32_t passo = (ms > 20) ? 20 : ms;
        vTaskDelay(passo / portTICK_PERIOD_MS + 1);
        ms = (ms > passo) ? (ms - passo) : 0;
        if (!safety_ok()) {
            return false;
        }
    }
    return safety_ok();
}

static void limpar_pergunta(void)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_s.aguardando = false;
    g_s.pergunta[0] = '\0';
    xSemaphoreGive(g_mutex);
}

/* Pergunta ao operador (pagina web ou botao FLASH = "sim"). */
static conf_t esperar_confirmacao(int i, const char *pergunta, uint32_t prazo_ms, bool aceita_botao)
{
    uint32_t t0 = test_manager_agora_ms();
    uint32_t desde = 0;
    bool     pressionado = false;

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_resposta = 0;
    g_s.aguardando = true;
    strncpy(g_s.pergunta, pergunta, sizeof(g_s.pergunta) - 1);
    g_s.pergunta[sizeof(g_s.pergunta) - 1] = '\0';
    xSemaphoreGive(g_mutex);
    definir(i, RES_AGUARDANDO_OPERADOR, "Aguardando resposta do operador.");

    while ((test_manager_agora_ms() - t0) < prazo_ms) {
        if (!safety_ok()) {
            limpar_pergunta();
            return CONF_ABORTADO;
        }
        if (g_resposta == 1) { limpar_pergunta(); return CONF_SIM; }
        if (g_resposta == 2) { limpar_pergunta(); return CONF_NAO; }
        if (aceita_botao) {
            if (board_botao_pressionado()) {
                if (!pressionado) {
                    pressionado = true;
                    desde = test_manager_agora_ms();
                } else if ((test_manager_agora_ms() - desde) >= 30) {   /* debounce */
                    limpar_pergunta();
                    return CONF_SIM;
                }
            } else {
                pressionado = false;
            }
        }
        vTaskDelay(1);
    }
    limpar_pergunta();
    return CONF_TEMPO;
}

/* --------------------------------- Testes ----------------------------- */

static void teste_sistema(int i)
{
    uint32_t livre = esp_get_free_heap_size();

    if (livre >= HEAP_MINIMO_TESTE) {
        definir(i, RES_APROVADO, "Memória livre: %u bytes (mínimo %u).",
                (unsigned)livre, (unsigned)HEAP_MINIMO_TESTE);
    } else {
        definir(i, RES_REPROVADO, "Memória livre baixa: %u bytes (mínimo %u).",
                (unsigned)livre, (unsigned)HEAP_MINIMO_TESTE);
    }
}

static void teste_led(int i)
{
    conf_t c;

    definir(i, RES_EXECUTANDO, "LED piscando 3 vezes...");
    for (int k = 0; k < 3; k++) {
        board_led_set(true);
        if (!pausa_segura(300)) { return; }
        board_led_set(false);
        if (!pausa_segura(300)) { return; }
    }
    c = esperar_confirmacao(i, "O LED azul da placa piscou 3 vezes? (botão FLASH = Sim)",
                            PRAZO_CONFIRMACAO_MS, true);
    switch (c) {
    case CONF_SIM:   definir(i, RES_APROVADO, "Operador confirmou o piscar do LED."); break;
    case CONF_NAO:   definir(i, RES_REPROVADO, "Operador não viu o LED piscar. Confira LED_ATIVO_BAIXO."); break;
    case CONF_TEMPO: definir(i, RES_INCONCLUSIVO, "Sem resposta do operador no prazo."); break;
    default:         break;
    }
}

static void teste_botao(int i)
{
    uint32_t t0;
    uint32_t desde = 0;
    bool     pressionado = false;

    if (board_botao_pressionado()) {
        definir(i, RES_INCONCLUSIVO, "Botão já estava pressionado. Solte e repita.");
        return;
    }
    definir(i, RES_EXECUTANDO, "Pressione o botão FLASH agora.");
    t0 = test_manager_agora_ms();
    while ((test_manager_agora_ms() - t0) < PRAZO_BOTAO_MS) {
        if (!safety_ok()) { return; }
        if (board_botao_pressionado()) {
            if (!pressionado) {
                pressionado = true;
                desde = test_manager_agora_ms();
            } else if ((test_manager_agora_ms() - desde) >= 30) {
                definir(i, RES_APROVADO, "Acionamento reconhecido em GPIO%d.", PIN_BOTAO_OPERADOR);
                return;
            }
        } else {
            pressionado = false;
        }
        vTaskDelay(1);
    }
    definir(i, RES_INCONCLUSIVO, "Botão não acionado em %d s.", PRAZO_BOTAO_MS / 1000);
}

#define MV_INT(x)  ((int)((x) / 1000))
#define MV_FRA(x)  ((int)((x) % 1000))

static void guardar_medida(const medida_t *m)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_s.med_valida = true;
    g_s.v_mv = m->tensao_mv;
    g_s.i_ma = m->corrente_ma;
    xSemaphoreGive(g_mutex);
}

static void teste_i2c(int i, const perfil_t *p)
{
    uint8_t achados[I2C_MAX_ACHADOS];
    char    lista[72];
    size_t  pos = 0;
    int     n, faltantes = 0;
    uint8_t primeiro = 0;

    definir(i, RES_EXECUTANDO, "Varrendo endereços 0x08 a 0x77...");
    n = i2c_bus_varrer(achados, I2C_MAX_ACHADOS);
    if (!safety_ok()) { return; }

    lista[0] = '\0';
    for (int k = 0; k < n && pos < sizeof(lista) - 6; k++) {
        pos += (size_t)snprintf(lista + pos, sizeof(lista) - pos, "%s0x%02X", k ? " " : "", achados[k]);
    }
    for (int k = 0; k < PERFIL_MAX_I2C && p->i2c_esperados[k] != 0; k++) {
        bool achou = false;
        for (int m = 0; m < n; m++) {
            if (achados[m] == p->i2c_esperados[k]) { achou = true; }
        }
        if (!achou) {
            if (faltantes == 0) { primeiro = p->i2c_esperados[k]; }
            faltantes++;
        }
    }
    if (p->i2c_esperados[0] == 0) {
        if (n == 0) {
            definir(i, RES_INCONCLUSIVO, "Nenhum dispositivo respondeu. Confira SDA/SCL, alimentação e pull-ups.");
        } else {
            definir(i, RES_INCONCLUSIVO, "Responderam: %s. O perfil não define endereços esperados.", lista);
        }
    } else if (faltantes == 0) {
        definir(i, RES_APROVADO, "Endereços esperados presentes. Responderam: %s.", lista);
    } else {
        definir(i, RES_REPROVADO, "Não respondeu: 0x%02X (+%d). Responderam: %s.",
                primeiro, faltantes - 1, n ? lista : "nenhum");
    }
}

static void teste_medidor(int i)
{
    medida_t m;

    definir(i, RES_EXECUTANDO, "Lendo o INA219...");
    if (measure_media(1, &m)) {
        definir(i, RES_APROVADO, "INA219 respondeu em 0x%02X.", INA219_ENDERECO);
    } else {
        definir(i, RES_REPROVADO, "INA219 não respondeu em 0x%02X (ou saturou). Confira SDA/SCL, 3,3 V e GND.",
                INA219_ENDERECO);
    }
}

static void teste_tensao(int i, const limites_t *lim)
{
    medida_t m;

    definir(i, RES_EXECUTANDO, "Medindo a tensão...");
    if (!measure_media(AMOSTRAS_TESTE, &m)) {
        definir(i, RES_INCONCLUSIVO, "Medidor indisponível: nenhum valor foi obtido.");
        return;
    }
    guardar_medida(&m);
    if (m.tensao_mv < TENSAO_PRESENTE_MV) {
        definir(i, RES_INCONCLUSIVO, "Sem tensão presente (%d,%03d V). Fonte desligada ou fora da entrada VIN+?",
                MV_INT(m.tensao_mv), MV_FRA(m.tensao_mv));
    } else if (lim->vmax_mv == 0) {
        definir(i, RES_INCONCLUSIVO, "Medido %d,%03d V. Defina os limites de tensão no painel para obter veredito.",
                MV_INT(m.tensao_mv), MV_FRA(m.tensao_mv));
    } else if (m.tensao_mv >= lim->vmin_mv && m.tensao_mv <= lim->vmax_mv) {
        definir(i, RES_APROVADO, "%d,%03d V dentro de %d,%03d a %d,%03d V.",
                MV_INT(m.tensao_mv), MV_FRA(m.tensao_mv), MV_INT(lim->vmin_mv), MV_FRA(lim->vmin_mv),
                MV_INT(lim->vmax_mv), MV_FRA(lim->vmax_mv));
    } else {
        definir(i, RES_REPROVADO, "%d,%03d V fora de %d,%03d a %d,%03d V.",
                MV_INT(m.tensao_mv), MV_FRA(m.tensao_mv), MV_INT(lim->vmin_mv), MV_FRA(lim->vmin_mv),
                MV_INT(lim->vmax_mv), MV_FRA(lim->vmax_mv));
    }
}

static void teste_corrente(int i, const limites_t *lim)
{
    medida_t m;

    definir(i, RES_EXECUTANDO, "Medindo a corrente...");
    if (!measure_media(AMOSTRAS_TESTE, &m)) {
        definir(i, RES_INCONCLUSIVO, "Medidor indisponível: nenhum valor foi obtido.");
        return;
    }
    guardar_medida(&m);
    if (m.tensao_mv < TENSAO_PRESENTE_MV) {
        definir(i, RES_INCONCLUSIVO, "Sem tensão presente: a corrente medida (%d mA) não é representativa.",
                (int)m.corrente_ma);
    } else if (lim->imax_ma == 0) {
        definir(i, RES_INCONCLUSIVO, "Medido %d mA. Defina o limite de corrente no painel para obter veredito.",
                (int)m.corrente_ma);
    } else if (m.corrente_ma > lim->imax_ma) {
        definir(i, RES_REPROVADO, "%d mA acima do máximo de %d mA.", (int)m.corrente_ma, (int)lim->imax_ma);
    } else if (m.corrente_ma < lim->imin_ma) {
        definir(i, RES_REPROVADO, "%d mA abaixo do mínimo de %d mA (carga desconectada?).",
                (int)m.corrente_ma, (int)lim->imin_ma);
    } else {
        definir(i, RES_APROVADO, "%d mA dentro de %d a %d mA.", (int)m.corrente_ma,
                (int)lim->imin_ma, (int)lim->imax_ma);
    }
}

/* Condicoes comuns aos testes com pontas/matriz: placa DESENERGIZADA.
 * Devolve true se pode medir; senao ja gravou o resultado. No modo
 * automatico nao ha operador: exige o INA219 confirmando ausencia de tensao. */
static bool exigir_desenergizada(int i, modo_t modo, const char *pergunta)
{
    medida_t m;
    bool tem_ina;

    if (power_ligada()) {
        definir(i, RES_INCONCLUSIVO, "Saída de potência LIGADA: desligue antes deste teste.");
        return false;
    }
    tem_ina = measure_media(1, &m);
    if (tem_ina && m.tensao_mv >= TENSAO_PRESENTE_MV) {
        definir(i, RES_INCONCLUSIVO, "Há %d,%03d V presentes: placa energizada. Teste não executado.",
                MV_INT(m.tensao_mv), MV_FRA(m.tensao_mv));
        return false;
    }
    if (modo == MODO_AUTOMATICO) {
        if (!tem_ina) {
            definir(i, RES_INCONCLUSIVO, "Sem INA219 não há como confirmar a placa desenergizada no modo automático.");
            return false;
        }
        return true;
    }
    {
        conf_t c = esperar_confirmacao(i, pergunta, PRAZO_CONFIRMACAO_MS, false);

        if (c == CONF_ABORTADO) { return false; }
        if (c != CONF_SIM) {
            definir(i, RES_INCONCLUSIVO, "Sem confirmação do operador: teste não executado.");
            return false;
        }
    }
    /* revalida depois da confirmacao: a placa pode ter sido ligada nesse intervalo */
    if (power_ligada() || (measure_media(1, &m) && m.tensao_mv >= TENSAO_PRESENTE_MV)) {
        definir(i, RES_INCONCLUSIVO, "Tensão detectada após a confirmação. Teste não executado.");
        return false;
    }
    return true;
}

static void teste_pontos(int i, const perfil_t *p, modo_t modo)
{
    int total = 0, ok = 0;
    char falha[64] = "";

    if (!matrix_presente()) {
        definir(i, RES_INCONCLUSIVO, "Matriz não detectada (PCF8574 em 0x%02X e 0x%02X).", MATRIZ_PCF_SEL, MATRIZ_PCF_EN);
        return;
    }
    if (!exigir_desenergizada(i, modo, "Placa/gabarito DESENERGIZADO e adaptador conectado à matriz?")) { return; }
    definir(i, RES_EXECUTANDO, "Medindo os pontos do adaptador...");
    for (int k = 0; k < PERFIL_MAX_PONTOS && p->pontos[k].nome != NULL; k++) {
        const ponto_t *pt = &p->pontos[k];
        int32_t r = 0;
        cont_estado_t est = matrix_medir_par(pt->a, pt->b, false, &r);
        bool bom;

        if (!safety_ok()) { return; }
        total++;
        if (est == CONT_ERRO) {
            definir(i, RES_INCONCLUSIVO, "Falha de leitura em %s (matriz/ADC).", pt->nome);
            return;
        }
        if (pt->aberto) {
            bom = (est == CONT_ABERTO);
        } else if (est == CONT_ABERTO) {
            bom = false;
        } else {
            bom = (r >= pt->rmin_ohm && (pt->rmax_ohm == 0 || r <= pt->rmax_ohm));
        }
        if (bom) {
            ok++;
        } else if (falha[0] == '\0') {
            if (est == CONT_ABERTO) {
                snprintf(falha, sizeof(falha), "%s: aberto", pt->nome);
            } else {
                snprintf(falha, sizeof(falha), "%s: %d ohm", pt->nome, (int)r);
            }
        }
    }
    if (total == 0) {
        definir(i, RES_INCONCLUSIVO, "O perfil não define pontos do adaptador.");
    } else if (ok == total) {
        definir(i, RES_APROVADO, "%d de %d pontos dentro do esperado.", ok, total);
    } else {
        definir(i, RES_REPROVADO, "%d de %d pontos ok. Primeira falha: %s. Calibrou MUX_OFFSET_OHM?",
                ok, total, falha);
    }
}

static void teste_varredura(int i, const perfil_t *p, const limites_t *lim, modo_t modo)
{
    int limiar = (lim->rmin_ohm > 0) ? (int)lim->rmin_ohm : VARREDURA_LIMIAR_PADRAO_OHM;
    int conta = 0, erros = 0;
    char lista[72];
    size_t pos = 0;

    if (!matrix_presente()) {
        definir(i, RES_INCONCLUSIVO, "Matriz não detectada (PCF8574 em 0x%02X e 0x%02X).", MATRIZ_PCF_SEL, MATRIZ_PCF_EN);
        return;
    }
    if (!exigir_desenergizada(i, modo, "Placa DESENERGIZADA, capacitores descarregados e adaptador conectado?")) { return; }
    definir(i, RES_EXECUTANDO, "Varrendo %d x %d pares...", MATRIZ_CANAIS, MATRIZ_CANAIS);
    lista[0] = '\0';
    for (int a = 0; a < MATRIZ_CANAIS; a++) {
        for (int b = 0; b < MATRIZ_CANAIS; b++) {
            int32_t r = 0;
            cont_estado_t est = matrix_medir_par((uint8_t)a, (uint8_t)b, true, &r);

            if (est == CONT_ERRO) { erros++; continue; }
            if (est == CONT_VALOR && r < limiar) {
                if (pos < sizeof(lista) - 14) {
                    pos += (size_t)snprintf(lista + pos, sizeof(lista) - pos, "%sA%d-B%d:%d",
                                            conta ? " " : "", a, b, (int)r);
                }
                conta++;
            }
        }
        if (!pausa_segura(1)) { matrix_liberar(); return; }   /* cede CPU e confere seguranca */
    }
    matrix_liberar();
    if (erros > 0) {
        definir(i, RES_INCONCLUSIVO, "%d leituras falharam (matriz/ADC). Resultado não confiável.", erros);
    } else if (p->varredura_definida) {
        definir(i, conta == p->condutivos_esperados ? RES_APROVADO : RES_REPROVADO,
                "%d pares abaixo de %d ohm (esperado %d). %s", conta, limiar,
                (int)p->condutivos_esperados, lista);
    } else {
        definir(i, RES_INCONCLUSIVO, "%d pares abaixo de %d ohm (sem contagem esperada no perfil). %s",
                conta, limiar, conta ? lista : "Nenhum par conduz.");
    }
}

static void teste_continuidade(int i, const limites_t *lim, modo_t modo)
{
    int32_t  r = 0;
    cont_estado_t est;
    bool     aberto, dentro;

    if (!exigir_desenergizada(i, modo, "Placa DESENERGIZADA, capacitores descarregados e pontas A/B posicionadas?")) {
        return;
    }
    definir(i, RES_EXECUTANDO, "Medindo entre as pontas A e B...");
    est = continuity_medir(&r);
    if (est == CONT_ERRO) {
        definir(i, RES_INCONCLUSIVO, "Falha ao ler o A0 (ADC). Nenhum valor obtido.");
        return;
    }
    aberto = (est == CONT_ABERTO);
    if (lim->rmin_ohm == 0 && lim->rmax_ohm == 0) {
        if (aberto) {
            definir(i, RES_INCONCLUSIVO, "Circuito ABERTO entre A e B. Defina a faixa de resistência esperada.");
        } else {
            definir(i, RES_INCONCLUSIVO, "Medido cerca de %d ohm. Defina a faixa esperada para obter veredito.", (int)r);
        }
        return;
    }
    /* aberto = resistencia infinita */
    dentro = aberto ? (lim->rmax_ohm == 0)
                    : (r >= lim->rmin_ohm && (lim->rmax_ohm == 0 || r <= lim->rmax_ohm));
    if (aberto) {
        definir(i, dentro ? RES_APROVADO : RES_REPROVADO, "Circuito aberto (maior que ~%d kohm). Esperado: %d a %d ohm (0 = sem máx.).",
                (CONT_RPD_OHM * (CONT_VDRIVE_MV - CONT_ABERTO_MV) / CONT_ABERTO_MV) / 1000,
                (int)lim->rmin_ohm, (int)lim->rmax_ohm);
    } else {
        definir(i, dentro ? RES_APROVADO : RES_REPROVADO, "Cerca de %d ohm. Esperado: %d a %d ohm (0 = sem máx.).",
                (int)r, (int)lim->rmin_ohm, (int)lim->rmax_ohm);
    }
}

/* Niveis logicos: a bancada nao tem conversor de nivel. Recusa se o INA219
 * enxerga mais que NIVEL_LOGICO_MAX_MV e, no modo assistido, exige a
 * confirmacao do operador de que o dispositivo e 3,3 V. */
static bool confirmar_3v3(int i, modo_t modo)
{
    medida_t m;
    bool tem = measure_media(1, &m);

    if (tem && m.tensao_mv > NIVEL_LOGICO_MAX_MV) {
        definir(i, RES_INCONCLUSIVO, "Há %d,%03d V no INA219: níveis lógicos podem passar de 3,3 V. Teste não executado.",
                MV_INT(m.tensao_mv), MV_FRA(m.tensao_mv));
        return false;
    }
    if (modo == MODO_AUTOMATICO) {
        return true;                       /* perfil validado define os niveis */
    }
    {
        conf_t c = esperar_confirmacao(i, "Dispositivo com lógica de 3,3 V (NUNCA 5 V) e GND comum ligado?",
                                       PRAZO_CONFIRMACAO_MS, false);

        if (c == CONF_ABORTADO) { return false; }
        if (c != CONF_SIM) {
            definir(i, RES_INCONCLUSIVO, "Sem confirmação de nível lógico: teste não executado.");
            return false;
        }
    }
    return true;
}

static void teste_i2c_reg(int i, const perfil_t *p, modo_t modo)
{
    int total = 0, ok = 0;
    char falha[80] = "";

    if (!confirmar_3v3(i, modo)) { return; }
    definir(i, RES_EXECUTANDO, "Lendo registradores...");
    for (int k = 0; k < PERFIL_MAX_I2C_REGS && p->i2c_regs[k].nome != NULL; k++) {
        const i2c_reg_t *r = &p->i2c_regs[k];
        uint16_t v = 0;
        esp_err_t e = i2c_bus_ler16(r->addr, r->reg, &v);

        total++;
        if (e == ESP_OK && (v & r->mascara) == (r->esperado & r->mascara)) {
            ok++;
        } else if (falha[0] == '\0') {
            if (e != ESP_OK) {
                snprintf(falha, sizeof(falha), "%s: sem resposta em 0x%02X", r->nome, r->addr);
            } else {
                snprintf(falha, sizeof(falha), "%s: leu 0x%04X, esperado 0x%04X", r->nome,
                         (unsigned)v, (unsigned)r->esperado);
            }
        }
    }
    if (total == 0) {
        definir(i, RES_INCONCLUSIVO, "O perfil não define registradores esperados.");
    } else if (ok == total) {
        definir(i, RES_APROVADO, "%d de %d registradores conferem com o esperado.", ok, total);
    } else {
        definir(i, RES_REPROVADO, "%d de %d ok. %s", ok, total, falha);
    }
}

static void teste_spi_id(int i, const perfil_t *p, modo_t modo)
{
    uint8_t tx[4] = { 0, 0, 0, 0 }, rx[4] = { 0, 0, 0, 0 };
    int n = p->spi_n;
    bool igual = true, tudo_ff = true, tudo_00 = true;

    if (p->spi_cmd == 0 || n < 1 || n > 3) {
        definir(i, RES_INCONCLUSIVO, "O perfil não define comando/ID SPI.");
        return;
    }
    if (!ioexp_presente()) {
        definir(i, RES_INCONCLUSIVO, "CS do SPI indisponível: PCF8574 em 0x%02X não responde.", MATRIZ_PCF_EN);
        return;
    }
    if (!confirmar_3v3(i, modo)) { return; }
    definir(i, RES_EXECUTANDO, "Lendo o ID por SPI...");
    tx[0] = p->spi_cmd;
    if (spi_soft_transferir(tx, rx, (size_t)(n + 1)) != ESP_OK) {
        definir(i, RES_INCONCLUSIVO, "Falha ao executar a transação SPI.");
        return;
    }
    for (int k = 0; k < n; k++) {
        if (rx[k + 1] != p->spi_esperado[k]) { igual = false; }
        if (rx[k + 1] != 0xFF) { tudo_ff = false; }
        if (rx[k + 1] != 0x00) { tudo_00 = false; }
    }
    if (igual) {
        definir(i, RES_APROVADO, "ID lido confere: %02X %02X %02X (primeiros %d byte(s) comparados).",
                rx[1], rx[2], rx[3], n);
    } else if (tudo_ff || tudo_00) {
        definir(i, RES_REPROVADO, "Sem resposta útil (leu %s): dispositivo desligado, MISO/CS/SCLK trocados ou sem GND.",
                tudo_ff ? "FF" : "00");
    } else {
        definir(i, RES_REPROVADO, "ID lido %02X %02X %02X difere do esperado (%02X...).",
                rx[1], rx[2], rx[3], p->spi_esperado[0]);
    }
}

static void teste_uart(int i, const perfil_t *p, modo_t modo)
{
    uart_medida_t m;

    if (p->uart_baud == 0) {
        definir(i, RES_INCONCLUSIVO, "O perfil não define baud esperado.");
        return;
    }
    if (!confirmar_3v3(i, modo)) { return; }
    definir(i, RES_EXECUTANDO, "Observando a linha por %d s... (o dispositivo precisa transmitir)", UART_JANELA_MS / 1000);
    uart_detectar(UART_JANELA_MS, &m);
    if (m.pulsos < 3 || m.baud == 0) {
        definir(i, RES_INCONCLUSIVO, "Poucos pulsos (%d). Nenhuma atividade confiável na linha RX (D6).", m.pulsos);
    } else if (m.limite) {
        definir(i, RES_INCONCLUSIVO, "Pulso no limite da resolução (~%d us): baud provavelmente acima do que o detector resolve.",
                (int)m.menor_us);
    } else {
        uint32_t d = (m.baud > p->uart_baud) ? (m.baud - p->uart_baud) : (p->uart_baud - m.baud);
        bool bate = (d * 100u <= p->uart_baud * (uint32_t)UART_TOLERANCIA_PCT);

        definir(i, bate ? RES_APROVADO : RES_REPROVADO, "Detectado ~%u baud (menor pulso %d us, %d pulsos). Esperado %u.",
                (unsigned)m.baud, (int)m.menor_us, m.pulsos, (unsigned)p->uart_baud);
    }
}

static void teste_oled(int i, const perfil_t *p, modo_t modo)
{
    static const struct { oled_padrao_t pad; const char *pergunta; const char *nome; } k_pad[] = {
        { OLED_TUDO_ACESO, "A tela está TODA acesa, sem pontos apagados?", "tudo aceso" },
        { OLED_APAGADO,    "A tela está TOTALMENTE apagada, sem pontos acesos?", "apagada" },
        { OLED_XADREZ,     "Aparece um xadrez regular, sem falhas ou linhas?", "xadrez" }
    };

    if (p->oled_addr == 0) {
        definir(i, RES_INCONCLUSIVO, "O perfil não define a tela.");
        return;
    }
    if (modo != MODO_ASSISTIDO) {
        definir(i, RES_INCONCLUSIVO, "Teste visual exige operador (somente modo assistido).");
        return;
    }
    if (!i2c_bus_responde(p->oled_addr)) {
        definir(i, RES_REPROVADO, "A tela não respondeu em 0x%02X (alimentada? SDA/SCL? endereço 0x3C/0x3D?).", p->oled_addr);
        return;
    }
    if (!confirmar_3v3(i, modo)) { return; }
    if (oled_iniciar(p->oled_addr) != ESP_OK) {
        definir(i, RES_REPROVADO, "Falha ao enviar a inicialização do SSD1306.");
        return;
    }
    for (int k = 0; k < 3; k++) {
        conf_t c;

        definir(i, RES_EXECUTANDO, "Mostrando o padrão: %s", k_pad[k].nome);
        if (oled_mostrar(p->oled_addr, p->oled_paginas, k_pad[k].pad) != ESP_OK) {
            definir(i, RES_REPROVADO, "Falha ao escrever o padrão %s na tela.", k_pad[k].nome);
            oled_desligar(p->oled_addr);
            return;
        }
        c = esperar_confirmacao(i, k_pad[k].pergunta, PRAZO_CONFIRMACAO_MS, false);
        if (c == CONF_ABORTADO) { oled_desligar(p->oled_addr); return; }
        if (c == CONF_NAO) {
            definir(i, RES_REPROVADO, "O operador reprovou o padrão: %s.", k_pad[k].nome);
            oled_desligar(p->oled_addr);
            return;
        }
        if (c != CONF_SIM) {
            definir(i, RES_INCONCLUSIVO, "Sem resposta do operador no padrão: %s.", k_pad[k].nome);
            oled_desligar(p->oled_addr);
            return;
        }
    }
    oled_desligar(p->oled_addr);
    definir(i, RES_APROVADO, "Os 3 padrões foram confirmados pelo operador (validação visual).");
}

/* --------------------------- Sequencia da sessao ---------------------- */

static resultado_t calcular_geral(void)
{
    bool reprovado = false, inconclusivo = false, algum = false;

    for (int i = 0; i < NUM_TESTES; i++) {
        resultado_t r = g_s.testes[i].resultado;
        if (r == RES_NAO_APLICAVEL) { continue; }
        algum = true;
        if (r == RES_INTERROMPIDO) { return RES_INTERROMPIDO; }
        if (r == RES_REPROVADO)    { reprovado = true; }
        if (r != RES_APROVADO && r != RES_REPROVADO) { inconclusivo = true; }
    }
    if (!algum || inconclusivo) { return reprovado ? RES_REPROVADO : RES_INCONCLUSIVO; }
    return reprovado ? RES_REPROVADO : RES_APROVADO;
}

static void tarefa_sessao(void *arg)
{
    uint16_t mascara;
    const perfil_t *p;
    modo_t modo;
    limites_t lim;

    (void)arg;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    p = perfil_obter(g_s.perfil);
    modo = g_s.modo;
    xSemaphoreGive(g_mutex);
    limites_obter(&lim);
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_s.lim = lim;
    xSemaphoreGive(g_mutex);
    mascara = (modo == MODO_ASSISTIDO) ? p->testes_assistido : p->testes_automatico;

    for (int i = 0; i < NUM_TESTES; i++) {
        resultado_t r;

        if (!(mascara & MASCARA(i))) {
            definir(i, RES_NAO_APLICAVEL, "Fora deste perfil/modo.");
            continue;
        }
        if (!safety_ok()) {
            definir(i, RES_INTERROMPIDO, "Não executado: bancada desarmada (%s).", safety_motivo_texto());
            continue;
        }
        switch (i) {
        case TESTE_SISTEMA:  teste_sistema(i); break;
        case TESTE_LED:      teste_led(i); break;
        case TESTE_BOTAO:    teste_botao(i); break;
        case TESTE_I2C:      teste_i2c(i, p); break;
        case TESTE_MEDIDOR:  teste_medidor(i); break;
        case TESTE_TENSAO:   teste_tensao(i, &lim); break;
        case TESTE_CORRENTE: teste_corrente(i, &lim); break;
        case TESTE_CONTINUIDADE: teste_continuidade(i, &lim, modo); break;
        case TESTE_PONTOS:   teste_pontos(i, p, modo); break;
        case TESTE_I2C_REG:  teste_i2c_reg(i, p, modo); break;
        case TESTE_SPI_ID:   teste_spi_id(i, p, modo); break;
        case TESTE_UART:     teste_uart(i, p, modo); break;
        case TESTE_OLED:     teste_oled(i, p, modo); break;
        case TESTE_VARREDURA: teste_varredura(i, p, &lim, modo); break;
        default: break;
        }
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        r = g_s.testes[i].resultado;
        xSemaphoreGive(g_mutex);
        if (r == RES_EXECUTANDO || r == RES_AGUARDANDO_OPERADOR ||
            (!safety_ok() && (i == TESTE_TENSAO || i == TESTE_CORRENTE))) {
            definir(i, RES_INTERROMPIDO, "Interrompido por segurança: %s.", safety_motivo_texto());
        }
    }

    board_io_estado_seguro();
    matrix_liberar();
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_s.fim_ms = test_manager_agora_ms();
    g_s.epoch = relogio_agora();
    g_s.geral = calcular_geral();
    g_s.estado = SESSAO_CONCLUIDA;
    result_logger_registrar(&g_s);
    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Sessao concluida: %s", resultado_texto(g_s.geral));
    vTaskDelete(NULL);
}

/* ------------------------------- Interface ---------------------------- */

void test_manager_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    memset(&g_s, 0, sizeof(g_s));
    g_s.estado = SESSAO_OCIOSA;
    g_s.geral = RES_PENDENTE;
    for (int i = 0; i < NUM_TESTES; i++) {
        g_s.testes[i].nome = k_nomes[i];
        g_s.testes[i].resultado = RES_PENDENTE;
        strcpy(g_s.testes[i].detalhe, "Aguardando início.");
    }
}

tm_erro_t test_manager_iniciar(int perfil, modo_t modo)
{
    const perfil_t *p = perfil_obter(perfil);

    if (p == NULL) { return TM_ERRO_PERFIL; }
    if (modo != MODO_ASSISTIDO && modo != MODO_AUTOMATICO) { return TM_ERRO_MODO_BLOQUEADO; }
    if (!safety_ok()) { return TM_ERRO_SEGURANCA; }
    if (modo == MODO_AUTOMATICO && !(p->validado && p->testes_automatico != 0)) {
        return TM_ERRO_MODO_BLOQUEADO;
    }

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    if (g_s.estado == SESSAO_EXECUTANDO) {
        xSemaphoreGive(g_mutex);
        return TM_ERRO_OCUPADO;
    }
    g_s.estado = SESSAO_EXECUTANDO;
    g_s.modo = modo;
    g_s.perfil = perfil;
    g_s.geral = RES_PENDENTE;
    g_s.inicio_ms = test_manager_agora_ms();
    g_s.fim_ms = 0;
    g_s.epoch = 0;
    g_s.aguardando = false;
    g_s.pergunta[0] = '\0';
    g_s.med_valida = false;
    g_s.v_mv = 0;
    g_s.i_ma = 0;
    g_resposta = 0;
    for (int i = 0; i < NUM_TESTES; i++) {
        g_s.testes[i].resultado = RES_PENDENTE;
        strcpy(g_s.testes[i].detalhe, "Aguardando...");
    }
    xSemaphoreGive(g_mutex);

    if (xTaskCreate(tarefa_sessao, "sessao", 4096, NULL, 5, NULL) != pdPASS) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_s.estado = SESSAO_OCIOSA;
        xSemaphoreGive(g_mutex);
        return TM_ERRO_INTERNO;
    }
    return TM_OK;
}

bool test_manager_ocupado(void)
{
    bool o;

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    o = (g_s.estado == SESSAO_EXECUTANDO);
    xSemaphoreGive(g_mutex);
    return o;
}

void test_manager_confirmar(bool sim)
{
    g_resposta = sim ? 1 : 2;
}

bool test_manager_rearmar(void)
{
    bool ocupado;

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    ocupado = (g_s.estado == SESSAO_EXECUTANDO);
    xSemaphoreGive(g_mutex);
    if (ocupado) { return false; }
    return safety_rearmar();
}

void test_manager_copiar(sessao_t *destino)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    *destino = g_s;
    xSemaphoreGive(g_mutex);
}

/* -------------------------------- Textos ------------------------------ */

const char *test_manager_nome(int teste)
{
    return (teste >= 0 && teste < NUM_TESTES) ? k_nomes[teste] : "?";
}

const char *resultado_texto(resultado_t r)
{
    switch (r) {
    case RES_EXECUTANDO:          return "EXECUTANDO";
    case RES_AGUARDANDO_OPERADOR: return "AGUARDANDO OPERADOR";
    case RES_APROVADO:            return "APROVADO";
    case RES_REPROVADO:           return "REPROVADO";
    case RES_INCONCLUSIVO:        return "INCONCLUSIVO";
    case RES_INTERROMPIDO:        return "INTERROMPIDO POR SEGURANÇA";
    case RES_NAO_APLICAVEL:       return "NÃO APLICÁVEL";
    default:                      return "PENDENTE";
    }
}

const char *resultado_codigo(resultado_t r)
{
    switch (r) {
    case RES_EXECUTANDO:          return "run";
    case RES_AGUARDANDO_OPERADOR: return "wait";
    case RES_APROVADO:            return "ok";
    case RES_REPROVADO:           return "fail";
    case RES_INCONCLUSIVO:        return "inc";
    case RES_INTERROMPIDO:        return "safe";
    case RES_NAO_APLICAVEL:       return "na";
    default:                      return "pend";
    }
}

const char *modo_texto(modo_t m)
{
    return (m == MODO_AUTOMATICO) ? "Automático" : "Assistido";
}

const char *tm_erro_texto(tm_erro_t e)
{
    switch (e) {
    case TM_OK:                return "ok";
    case TM_ERRO_OCUPADO:      return "Já existe um teste em execução.";
    case TM_ERRO_SEGURANCA:    return "Bancada desarmada. Rearme antes de testar.";
    case TM_ERRO_PERFIL:       return "Perfil inválido.";
    case TM_ERRO_MODO_BLOQUEADO: return "Modo automático bloqueado: perfil não validado.";
    default:                   return "Erro interno.";
    }
}
