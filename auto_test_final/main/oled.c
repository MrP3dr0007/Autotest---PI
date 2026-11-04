/*
 * oled.c - SSD1306 128 x (32|64) por I2C. Sequencia de inicializacao padrao
 * (carga interna 0x8D/0x14). Controladores SH1106 usam 132 colunas e podem
 * mostrar deslocamento de 2 pixels: isso nao e defeito do painel.
 * A decisao aprovado/reprovado e do OPERADOR (inspecao visual).
 */
#include "oled.h"
#include "i2c_bus.h"
#include "config.h"

static const uint8_t k_init[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
    0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
};

static esp_err_t comandos(uint8_t addr, const uint8_t *c, int n)
{
    while (n > 0) {
        int t = (n > 16) ? 16 : n;
        esp_err_t e = i2c_bus_escrever_ctrl(addr, 0x00, c, t);

        if (e != ESP_OK) { return e; }
        c += t;
        n -= t;
    }
    return ESP_OK;
}

esp_err_t oled_iniciar(uint8_t addr)
{
    return comandos(addr, k_init, (int)sizeof(k_init));
}

esp_err_t oled_mostrar(uint8_t addr, uint8_t paginas, oled_padrao_t padrao)
{
    uint8_t area[6];
    uint8_t bloco[16];

    if (paginas != 4 && paginas != 8) {
        return ESP_ERR_INVALID_STATE;
    }
    area[0] = 0x21; area[1] = 0; area[2] = OLED_COLUNAS - 1;     /* colunas */
    area[3] = 0x22; area[4] = 0; area[5] = (uint8_t)(paginas - 1); /* paginas */
    if (comandos(addr, area, 6) != ESP_OK) {
        return ESP_FAIL;
    }
    for (int pg = 0; pg < paginas; pg++) {
        for (int col = 0; col < OLED_COLUNAS; col += 16) {
            for (int k = 0; k < 16; k++) {
                uint8_t v;

                switch (padrao) {
                case OLED_TUDO_ACESO: v = 0xFF; break;
                case OLED_APAGADO:    v = 0x00; break;
                default:              v = ((((col + k) >> 3) + pg) & 1) ? 0xFF : 0x00; break;
                }
                bloco[k] = v;
            }
            if (i2c_bus_escrever_ctrl(addr, 0x40, bloco, 16) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    }
    return ESP_OK;
}

void oled_desligar(uint8_t addr)
{
    static const uint8_t off[] = { 0xAE };

    comandos(addr, off, 1);
}
