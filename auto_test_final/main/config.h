/*
 * config.h - Configuracao central da bancada Auto Test (Etapa 3)
 * Todos os pinos usam numeracao GPIO do ESP8266 (nao a numeracao D0..D8 da placa).
 */
#ifndef CONFIG_H
#define CONFIG_H

#define FW_VERSAO "1.0.0"

/* ----------------------- Pinos usados ----------------------------------- */
/* LED azul do modulo ESP-12 (GPIO2 / D4). Na maioria das placas e ativo em
 * nivel BAIXO. CONFIRME na sua placa; se acender ao contrario, mude para 0. */
#define PIN_LED_STATUS        2
#define LED_ATIVO_BAIXO       1

/* Botao FLASH da placa (GPIO0 / D3), ativo em nivel BAIXO. */
#define PIN_BOTAO_OPERADOR    0

/* Barramento I2C (D2 = SDA, D1 = SCL). A partir da Etapa 2 o jumper de
 * loopback D2-D1 da Etapa 1 deve ser REMOVIDO. */
#define PIN_I2C_SDA           4
#define PIN_I2C_SCL           5

/* Chave de potencia (rele/MOSFET) em GPIO16 (D0). Deixe 0 ate montar o
 * modulo: com 0 o firmware NAO mexe em GPIO16 e a saida fica "nao instalada".
 * Use contato NORMALMENTE ABERTO: sem energia ou com o ESP travado = desligado. */
#define POTENCIA_INSTALADA    0
#define PIN_POTENCIA          16
#define POTENCIA_ATIVO_ALTO   1       /* 0 se o modulo de rele liga em nivel BAIXO */

/* Ponta de continuidade A: GPIO14 (D5) -> resistor R1 -> ponta A.
 * Ponta B -> A0 (e resistor de pull-down externo para GND). Etapa 4 trocara
 * isto por multiplexador; D5 volta a ser SCLK do SPI na Etapa 5.            */
#define PIN_PONTA_A           14
#define CONT_R1_OHM           1000    /* resistor serie da ponta A           */
#define CONT_RPD_OHM          9700    /* 10 k externo || ~320 k da NodeMCU   */
#define CONT_VDRIVE_MV        3300
#define A0_FUNDO_ESCALA_MV    3200    /* NodeMCU: divisor 220k/100k; calibre */
#define CONT_ABERTO_MV        20      /* abaixo disso = circuito aberto      */

/* ----------------- Matriz de pontos de teste (Etapa 4) ------------------ */
/* Dois multiplexadores CD74HC4067 (16 canais cada) comandados por DOIS
 * expansores PCF8574 no I2C (poupa GPIOs):
 *   PCF 0x20: P0..P3 = S0..S3 do mux A | P4..P7 = S0..S3 do mux B
 *   PCF 0x21: P0 = /EN do mux A | P1 = /EN do mux B (nivel ALTO = desabilitado)
 * O PCF8574 liga com todas as saidas em ALTO: os multiplexadores nascem
 * desabilitados. COM do mux A = ponta A (GPIO14 via R1); COM do mux B = ponta
 * B (A0 + pull-down). Alimente os muxes em 3,3 V. */
#define MATRIZ_PCF_SEL        0x20
#define MATRIZ_PCF_EN         0x21
#define MATRIZ_CANAIS         16
/* Resistencia serie dos muxes/resistores de canal, medida com um jumper entre
 * um canal A e um canal B (veja o README) e SUBTRAIDA das leituras. */
#define MUX_OFFSET_OHM        0
#define VARREDURA_LIMIAR_PADRAO_OHM 100

/* ------------------- Comunicacao: SPI e UART (Etapa 5) ------------------ */
/* SPI por SOFTWARE (bit-bang, modo 0, ~100-200 kHz): serve para ler ID e
 * registradores; nao serve para telas SPI grandes nem alta velocidade.
 * O CS vem do expansor PCF8574 #2 (P2, ativo em BAIXO, repouso ALTO).     */
#define PIN_SPI_SCLK          13      /* D7 */
#define PIN_SPI_MOSI          15      /* D8 (nivel baixo no boot: nao ligar a nada que puxe para cima) */
#define PIN_SPI_MISO          12      /* D6 */
#define IOEXP_BIT_EN_MUX      0x03    /* P0,P1 do PCF #2: /EN dos multiplexadores */
#define IOEXP_BIT_CS_SPI      0x04    /* P2 do PCF #2 */
#define SPI_MEIO_PERIODO_US   2

/* UART: detector de baud por SOFTWARE no mesmo GPIO12 do MISO (nao simultaneo).
 * Mede o menor pulso em BAIXO. EXPERIMENTAL: calibre UART_US_POR_AMOSTRA com um
 * transmissor de baud conhecido (ex.: adaptador USB-serial a 9600). */
#define PIN_UART_RX           12
#define UART_US_POR_AMOSTRA   12      /* periodo efetivo de amostragem (us) */
#define UART_TOLERANCIA_PCT   15
#define UART_JANELA_MS        3000

/* Niveis logicos: acima disto a bancada recusa os testes de comunicacao. */
#define NIVEL_LOGICO_MAX_MV   3600

/* Tela OLED SSD1306 (I2C): enderecos comuns 0x3C/0x3D. */
#define OLED_COLUNAS          128

/* ----------- Pinos RESERVADOS para etapas futuras (nao usar) ------------ */
/* GPIO12/13/14 : SPI (MISO/MOSI/SCLK) na Etapa 5 e 6 (GPIO14 = ponta A hoje) */
/* GPIO15       : CS do SPI; precisa ficar em nivel BAIXO no boot            */
/* GPIO1/GPIO3  : UART0 (console USB) - nao usar                             */
/* GPIO6..GPIO11: ligados a flash - nunca usar                               */

/* ------------------------- Medidor INA219 (I2C) ------------------------- */
#define INA219_ENDERECO       0x40    /* A0=A1=GND (padrao dos modulos)      */
#define INA219_SHUNT_MOHM     100     /* shunt R100 = 0,1 ohm (padrao GY-219) */

/* Tetos ABSOLUTOS do hardware. O painel nunca aceita limites acima disso.
 * INA219: barramento ate 26 V e shunt ate +-320 mV (3,2 A com 0,1 ohm).    */
#define TETO_TENSAO_MV        24000
#define TETO_CORRENTE_MA      3000

#define MEDIDA_PERIODO_MS     100     /* periodo do monitor de medicao       */
#define MEDIDA_CONFIRMACOES   2       /* leituras seguidas para disparar (~200 ms) */
#define MEDIDA_FALHAS_MAX     5       /* falhas I2C seguidas = indisponivel  */
#define TENSAO_PRESENTE_MV    300     /* abaixo disso: sem alimentacao       */

/* Calibracao (compare com um multimetro e ajuste). 1000 = ganho 1,000.
 * valor_calibrado = valor_bruto * GANHO / 1000 + OFFSET                     */
#define CAL_TENSAO_GANHO      1000
#define CAL_TENSAO_OFFSET_MV  0
#define CAL_CORRENTE_GANHO    1000
#define CAL_CORRENTE_OFFSET_MA 0

/* ------------------------------- Wi-Fi ---------------------------------- */
#define WIFI_SSID             "AutoTest-ESP"
#define WIFI_SENHA            "autotest123"   /* minimo de 8 caracteres */
#define WIFI_MAX_CLIENTES     4

/* ----------------------------- Seguranca -------------------------------- */
#define HEAP_SEGURANCA_BYTES  8000     /* abaixo disso a bancada desarma */
#define HEAP_MINIMO_TESTE     20000    /* criterio do teste de sistema   */

/* ------------------------------- Testes --------------------------------- */
#define PRAZO_BOTAO_MS        8000
#define PRAZO_CONFIRMACAO_MS  30000
#define MAX_HISTORICO         8
#define AMOSTRAS_TESTE        8        /* amostras na media de V e I     */

/* Estes perfis so liberam o modo automatico depois que voce executar o plano
 * de validacao e mudar o valor para 1. */
#define PERFIL_AUTODIAG_VALIDADO 0
#define PERFIL_REF5V_VALIDADO    0
#define PERFIL_CURTO_VALIDADO    0
#define PERFIL_MATRIZ_VALIDADO   0
#define PERFIL_COMM_VALIDADO     0

#endif
