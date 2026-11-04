/*
 * power.c - Liga/desliga a saida de potencia com pre-condicoes de seguranca
 *
 * So liga se: modulo instalado, seguranca armada, INA219 respondendo (para
 * monitorar), limites de tensao e corrente MAXIMOS definidos, nenhum teste em
 * andamento. Desliga a qualquer momento. Depois de ligada, o monitor de
 * measure.c corta se algum limite for violado.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power.h"
#include "board_io.h"
#include "measure.h"
#include "safety.h"
#include "test_manager.h"
#include "config.h"

bool power_instalada(void)
{
    return POTENCIA_INSTALADA != 0;
}

bool power_ligada(void)
{
    return board_potencia_ligada();
}

const char *power_ligar(void)
{
    limites_t l;

    if (!power_instalada())          { return "Módulo de potência não instalado (POTENCIA_INSTALADA em config.h)."; }
    if (!safety_ok())                { return "Bancada desarmada. Rearme antes de ligar a saída."; }
    if (!measure_presente())         { return "INA219 ausente: não ligo a saída sem monitorar tensão e corrente."; }
    if (test_manager_ocupado())      { return "Há um teste em execução."; }
    limites_obter(&l);
    if (l.vmax_mv <= 0 || l.imax_ma <= 0) {
        return "Defina a tensão máxima e a corrente máxima antes de ligar.";
    }
    board_potencia_set(true);
    vTaskDelay(300 / portTICK_PERIOD_MS);          /* deixa o monitor ver a partida */
    if (!safety_ok()) {
        board_potencia_set(false);
        return "A segurança disparou na partida. Saída desligada.";
    }
    return NULL;
}

void power_desligar(void)
{
    board_potencia_set(false);
}
