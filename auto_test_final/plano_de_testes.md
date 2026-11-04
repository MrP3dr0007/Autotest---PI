# Plano de testes - Auto Test v1.0 (execute na ordem; marque cada item)

Use fonte de bancada com limite de corrente BAIXO (ex.: 100 mA) e carga conhecida. Multimetro como referencia.

| # | Modulo | Procedimento | Criterio de aceitacao |
|---|---|---|---|
| 1 | Build | make menuconfig; make -j4 flash monitor | compila sem erro; log mostra "Pronto" |
| 2 | Wi-Fi/painel | conectar em AutoTest-ESP; abrir 192.168.4.1 | painel abre em portugues |
| 3 | LED/botao | perfil AUTODIAG-E2, modo assistido | LED pisca; botao FLASH detectado |
| 4 | INA219 | fonte 5 V + resistor conhecido; ver Medicoes | V e I dentro de ~5% do multimetro; ajustar CAL_* |
| 5 | Limites | definir vmax=5,5 V e imax=50 mA; forcar 100 mA | bancada desarma e mostra o motivo |
| 6 | Chave de potencia | POTENCIA_INSTALADA=1; ligar saida pelo painel | rele fecha so com limites definidos; multimetro confirma OFF durante o boot |
| 7 | Corte | com saida ligada, exceder imax | saida abre em <~0,5 s; rearme exige condicao normal |
| 8 | Continuidade | resistores 0/100/1k entre as pontas A e B (desenergizado) | valores coerentes; calibrar CONT_* |
| 9 | Recusa de energizada | aplicar 5 V na placa e iniciar CURTO-TRILHO | teste recusa e fica INCONCLUSIVO |
| 10 | Matriz | gabarito: jumper A0-B0, 1 k em A2-B2, A1-B1 aberto; calibrar MUX_OFFSET_OHM | MATRIZ-AUTODIAG: pontos e varredura conferem |
| 11 | I2C ID | perfil I2C-CONHECIDOS com INA219 | APROVADO |
| 12 | SPI | flash W25Qxx em 3,3 V; perfil SPI-FLASH-ID | ID 0xEF confere; desconectar MISO deve dar REPROVADO |
| 13 | UART | USB-serial 3,3 V a 9600 enviando texto no D6; perfil UART-9600 | baud ~9600; calibrar UART_US_POR_AMOSTRA |
| 14 | OLED | SSD1306 0x3C; perfil TELA-OLED-I2C | 3 padroes confirmados; pixel morto deve ser REPROVADO |
| 15 | Relatorio/CSV | abrir "relatorio" e exportar CSV | dados batem com o painel; data aparece |
| 16 | Persistencia | reiniciar a placa | historico anterior reaparece |
Apos tudo OK para um perfil, mude PERFIL_*_VALIDADO para 1 e valide o modo automatico.
