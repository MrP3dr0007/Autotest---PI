/* measure.h - Medicao de tensao e corrente (INA219) e monitor de limites */
#ifndef MEASURE_H
#define MEASURE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     disponivel;     /* INA219 respondendo agora           */
    int32_t  tensao_mv;      /* tensao do barramento (calibrada)   */
    int32_t  corrente_ma;    /* modulo da corrente (calibrada)     */
} medida_t;

typedef struct {
    int32_t vmin_mv;         /* 0 = nao definido */
    int32_t vmax_mv;
    int32_t imin_ma;
    int32_t imax_ma;
    int32_t rmin_ohm;        /* continuidade: faixa esperada entre as pontas */
    int32_t rmax_ohm;        /* rmin=rmax=0: nao definido; rmax=0: sem maximo */
} limites_t;

void      measure_init(void);
bool      measure_presente(void);                 /* INA219 detectado/ativo */
void      measure_ultima(medida_t *m);            /* ultima leitura do monitor */
/* Media de n leituras diretas (n >= 1). Devolve false se o medidor falhar. */
bool      measure_media(int n, medida_t *m);
bool      measure_em_risco(void);                 /* limite violado agora? */

/* Limites ativos (persistidos na flash). Devolve nullptr-safe copia. */
void      limites_obter(limites_t *l);
/* Valida e salva. Devolve NULL se ok, ou texto do erro. */
const char *limites_definir(const limites_t *l);

#endif
