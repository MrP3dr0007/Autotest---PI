/*
 * profile_manager.c - Perfis de teste
 *
 * Adaptadores: a lista "pontos" mapeia nomes de pontos do conector para canais da
 * matriz. Para uma placa nova, copie o perfil MATRIZ-AUTODIAG e troque os pontos.
 * Resistencias: rmin/rmax em ohm entre as pontas A e B (rmax=0 = sem maximo).
 * Os limites_padrao sao apenas SUGESTOES exibidas no painel. Quem vale na
 * hora do teste sao os limites ativos que o operador aplicou (ajustaveis).
 * Perfil com valor 0 em vmax/imax = "o operador precisa definir".
 */
#include <stddef.h>
#include "profile_manager.h"
#include "config.h"

static const perfil_t k_perfis[] = {
    {
        "DESCONHECIDA",
        "Placa desconhecida (somente assistido)",
        "Varredura I2C e medição de tensão e corrente da fonte externa, comparadas com os limites que VOCÊ definir.",
        "Nada é energizado pela bancada. Ajuste a fonte de bancada com limite de corrente BAIXO e defina os limites antes.",
        false,
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_MEDIDOR) |
        MASCARA(TESTE_TENSAO) | MASCARA(TESTE_CORRENTE) | MASCARA(TESTE_CONTINUIDADE) |
        MASCARA(TESTE_VARREDURA),
        0,
        { 0, 0, 0, 0, 0, 0 },
        { 0 }
    },
    {
        "AUTODIAG-E2",
        "Autodiagnóstico da bancada (Etapa 2)",
        "Sistema, LED, botão FLASH, scan I2C e presença do INA219.",
        "Remova o jumper D2-D1 da Etapa 1. O INA219 precisa estar em D1/D2 (SCL/SDA), 3,3 V e GND.",
        PERFIL_AUTODIAG_VALIDADO ? true : false,
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_LED) | MASCARA(TESTE_BOTAO) |
        MASCARA(TESTE_I2C) | MASCARA(TESTE_MEDIDOR),
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_MEDIDOR),
        { 0, 0, 0, 0, 0, 0 },
        { INA219_ENDERECO }
    },
    {
        "REF-5V",
        "Referência 5 V com carga conhecida (calibração)",
        "Mede uma fonte de 5 V ligada a uma carga resistiva conhecida e compara com os limites.",
        "Use fonte de bancada em 5,00 V com limite de corrente de 0,2 A ou menos e um resistor de valor conhecido.",
        PERFIL_REF5V_VALIDADO ? true : false,
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_MEDIDOR) | MASCARA(TESTE_TENSAO) | MASCARA(TESTE_CORRENTE),
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_MEDIDOR) | MASCARA(TESTE_TENSAO) | MASCARA(TESTE_CORRENTE),
        { 4750, 5250, 0, 200, 0, 0 },
        { INA219_ENDERECO }
    },
    {
        "CURTO-TRILHO",
        "Checagem de curto trilho × GND (antes de energizar)",
        "Mede a resistência entre um trilho de alimentação e o GND da placa DESENERGIZADA. Abaixo do mínimo = possível curto.",
        "Só com a placa desligada e capacitores descarregados. Ponta A no trilho, ponta B no GND. Resolução de dezenas de ohms.",
        PERFIL_CURTO_VALIDADO ? true : false,
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_CONTINUIDADE),
        0,
        { 0, 0, 0, 0, 500, 0 },
        { 0 }
    },
    {
        "MATRIZ-AUTODIAG",
        "Autodiagnóstico da matriz de pontos (Etapa 4)",
        "Confere a matriz com um gabarito: jumper A0-B0, resistor de 1 kohm em A2-B2 e A1-B1 aberto.",
        "Placa DESENERGIZADA. Antes, calibre MUX_OFFSET_OHM em config.h (jumper A0-B0 lido com offset 0).",
        PERFIL_MATRIZ_VALIDADO ? true : false,
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_PONTOS) | MASCARA(TESTE_VARREDURA),
        MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_PONTOS) | MASCARA(TESTE_VARREDURA),
        { 0, 0, 0, 0, VARREDURA_LIMIAR_PADRAO_OHM, 0 },
        { INA219_ENDERECO, MATRIZ_PCF_SEL, MATRIZ_PCF_EN },
        {
            { "A0-B0 jumper", 0, 0, 0, 150, false },
            { "A1-B1 aberto", 1, 1, 0, 0, true },
            { "A2-B2 1 kohm", 2, 2, 700, 1400, false },
            { NULL, 0, 0, 0, 0, false }
        },
        true, 1
    },
    {
        .id = "I2C-CONHECIDOS",
        .nome = "Dispositivos I2C conhecidos (ID por registrador)",
        .descricao = "Lê o registrador de identificação de dispositivos I2C conhecidos e compara com o valor esperado.",
        .alertas = "Dispositivo alimentado em 3,3 V e com SDA/SCL em 3,3 V. Nunca 5 V. Edite i2c_regs para o seu dispositivo.",
        .validado = PERFIL_COMM_VALIDADO ? true : false,
        .testes_assistido = MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_I2C_REG),
        .testes_automatico = MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_I2C_REG),
        .i2c_esperados = { INA219_ENDERECO },
        /* Exemplos a conferir no datasheet do SEU componente antes de usar:
         *   MPU-6050: { "MPU6050 WHO_AM_I", 0x68, 0x75, 0x6800, 0xFF00 }
         *   BME280  : { "BME280 chip id",   0x76, 0xD0, 0x6000, 0xFF00 } */
        .i2c_regs = { { "INA219 configuração", INA219_ENDERECO, 0x00, 0x399F, 0xFFFF } }
    },
    {
        .id = "SPI-FLASH-ID",
        .nome = "Flash SPI: ID JEDEC (comando 0x9F)",
        .descricao = "Lê o ID JEDEC de uma memória flash SPI (ex.: família W25Qxx, fabricante 0xEF) e compara.",
        .alertas = "Flash alimentada em 3,3 V. SCLK=D7, MOSI=D8, MISO=D6, CS=PCF 0x21 P2. Fios curtos (<15 cm). Confira o ID no datasheet.",
        .validado = PERFIL_COMM_VALIDADO ? true : false,
        .testes_assistido = MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_SPI_ID),
        .testes_automatico = MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_SPI_ID),
        .i2c_esperados = { MATRIZ_PCF_EN },
        .spi_cmd = 0x9F, .spi_n = 1, .spi_esperado = { 0xEF }
    },
    {
        .id = "UART-9600",
        .nome = "UART: detecção de baud (9600) - experimental",
        .descricao = "Observa a linha TX do dispositivo no D6 e estima o baud pelo menor pulso.",
        .alertas = "Só TX do dispositivo para o D6, em 3,3 V, GND comum. Exige que o dispositivo transmita texto. Calibre UART_US_POR_AMOSTRA.",
        .validado = false,
        .testes_assistido = MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_UART),
        .testes_automatico = 0,
        .uart_baud = 9600
    },
    {
        .id = "TELA-OLED-I2C",
        .nome = "Tela OLED SSD1306 (I2C, inspeção visual)",
        .descricao = "Acende tudo, apaga tudo e mostra um xadrez; você confirma cada padrão olhando a tela.",
        .alertas = "Módulo OLED em 3,3 V no mesmo SDA/SCL. Endereço 0x3C (ou 0x3D). SH1106 pode mostrar 2 px de deslocamento.",
        .validado = false,
        .testes_assistido = MASCARA(TESTE_SISTEMA) | MASCARA(TESTE_I2C) | MASCARA(TESTE_OLED),
        .testes_automatico = 0,
        .i2c_esperados = { 0x3C },
        .oled_addr = 0x3C, .oled_paginas = 8
    }
};

int perfil_total(void)
{
    return (int)(sizeof(k_perfis) / sizeof(k_perfis[0]));
}

const perfil_t *perfil_obter(int indice)
{
    if (indice < 0 || indice >= perfil_total()) {
        return NULL;
    }
    return &k_perfis[indice];
}
