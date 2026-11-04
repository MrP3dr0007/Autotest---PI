/*
 * i2c_bus.c - Barramento I2C mestre (ESP8266_RTOS_SDK v3.4, driver/i2c.h)
 *
 * Um mutex protege o barramento: o monitor de medicao, os testes e a
 * varredura podem rodar em tarefas diferentes. A frequencia do I2C no
 * ESP8266 e gerada por software e nao e configuravel neste driver.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "config.h"

static const char *TAG = "I2C";
#define PORTA_I2C   I2C_NUM_0
#define PRAZO_I2C   (100 / portTICK_PERIOD_MS)

static SemaphoreHandle_t g_mutex;
static bool g_pronto;

esp_err_t i2c_bus_init(void)
{
    i2c_config_t cfg;
    esp_err_t e;

    if (g_pronto) {
        return ESP_OK;
    }
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = PIN_I2C_SDA;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;      /* pull-up interno e fraco: */
    cfg.scl_io_num = PIN_I2C_SCL;                /* o modulo INA219 ja traz  */
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;      /* pull-ups externos.       */
    cfg.clk_stretch_tick = 300;

    e = i2c_driver_install(PORTA_I2C, cfg.mode);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "driver_install falhou: %d", (int)e);
        return e;
    }
    e = i2c_param_config(PORTA_I2C, &cfg);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "param_config falhou: %d", (int)e);
        return e;
    }
    g_pronto = true;
    return ESP_OK;
}

static esp_err_t executar(i2c_cmd_handle_t cmd)
{
    return i2c_master_cmd_begin(PORTA_I2C, cmd, PRAZO_I2C);
}

esp_err_t i2c_bus_ler16(uint8_t addr, uint8_t reg, uint16_t *valor)
{
    uint8_t b[2] = {0, 0};
    esp_err_t e;
    i2c_cmd_handle_t cmd;

    if (!g_pronto || valor == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_READ), true);
    i2c_master_read_byte(cmd, &b[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &b[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    e = executar(cmd);
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_mutex);
    if (e == ESP_OK) {
        *valor = (uint16_t)((b[0] << 8) | b[1]);
    }
    return e;
}

esp_err_t i2c_bus_escrever16(uint8_t addr, uint8_t reg, uint16_t valor)
{
    esp_err_t e;
    i2c_cmd_handle_t cmd;

    if (!g_pronto) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, (uint8_t)(valor >> 8), true);
    i2c_master_write_byte(cmd, (uint8_t)(valor & 0xFF), true);
    i2c_master_stop(cmd);
    e = executar(cmd);
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_mutex);
    return e;
}

esp_err_t i2c_bus_escrever8(uint8_t addr, uint8_t valor)
{
    esp_err_t e;
    i2c_cmd_handle_t cmd;

    if (!g_pronto) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_write_byte(cmd, valor, true);
    i2c_master_stop(cmd);
    e = executar(cmd);
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_mutex);
    return e;
}

esp_err_t i2c_bus_escrever_ctrl(uint8_t addr, uint8_t ctrl, const uint8_t *dados, int n)
{
    esp_err_t e;
    i2c_cmd_handle_t cmd;

    if (!g_pronto || n < 0 || n > 32 || (n > 0 && dados == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_write_byte(cmd, ctrl, true);
    for (int k = 0; k < n; k++) {
        i2c_master_write_byte(cmd, dados[k], true);
    }
    i2c_master_stop(cmd);
    e = executar(cmd);
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_mutex);
    return e;
}

bool i2c_bus_responde(uint8_t addr)
{
    esp_err_t e;
    i2c_cmd_handle_t cmd;

    if (!g_pronto) {
        return false;
    }
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_stop(cmd);
    e = executar(cmd);
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_mutex);
    return e == ESP_OK;
}

int i2c_bus_varrer(uint8_t *achados, int max)
{
    int n = 0;

    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_bus_responde(a) && n < max) {
            achados[n++] = a;
        }
        vTaskDelay(1);        /* nao monopoliza a CPU (watchdog / Wi-Fi) */
    }
    return n;
}
