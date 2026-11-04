# AUTO TEST v1.0 - Bancada modular de diagnostico (ESP8266, C puro, ESP8266_RTOS_SDK v3.4)

Resumo das etapas (todas no mesmo firmware):
1. Base: Wi-Fi AutoTest-ESP, painel em portugues, LED, botao, maquina de estados, parada de emergencia.
2. Medicao: INA219 (tensao/corrente reais), scan I2C, limites ajustaveis salvos na flash.
3. Protecao: chave de potencia (GPIO16), corte automatico, continuidade com a placa desenergizada.
4. Pontos de teste: 2 x CD74HC4067 + 2 x PCF8574, testes PONTOS e VARREDURA 16x16.
5. Comunicacao: registradores I2C conhecidos, ID SPI (bit-bang), baud UART (detector EXPERIMENTAL).
6. Telas: OLED SSD1306 por I2C com validacao VISUAL; demais telas = modulos externos (ver abaixo).
7. Relatorios: historico persistente (NVS), CSV, relatorio imprimivel por diagnostico, comparacao com a leitura anterior.

## Mapa de pinos (GPIO do ESP8266)
| Pino NodeMCU | GPIO | Uso |
|---|---|---|
| D0 | 16 | chave de potencia (POTENCIA_INSTALADA=1) |
| D1 / D2 | 5 / 4 | I2C SCL / SDA (INA219, PCF8574 x2, OLED) |
| D3 | 0 | botao FLASH (nao ligar nada que force nivel baixo no boot) |
| D4 | 2 | LED azul |
| D5 | 14 | ponta A da continuidade (via 1 kohm) |
| D6 | 12 | MISO do SPI / RX do detector UART (nao simultaneos) |
| D7 | 13 | SCLK do SPI |
| D8 | 15 | MOSI do SPI (precisa estar em nivel baixo no boot) |
| A0 | ADC | ponta B da continuidade (pull-down 10 kohm) |
| PCF 0x20 | - | P0..P3 = S0..S3 mux A; P4..P7 = S0..S3 mux B |
| PCF 0x21 | - | P0,P1 = /EN dos muxes; P2 = CS do SPI |

## Perfis incluidos
DESCONHECIDA (somente assistido), AUTODIAG-E2, REF-5V, CURTO-TRILHO, MATRIZ-AUTODIAG,
I2C-CONHECIDOS, SPI-FLASH-ID, UART-9600, TELA-OLED-I2C.
Nenhum perfil vem VALIDADO: o modo automatico fica bloqueado ate voce executar o
plano_de_testes.md e mudar PERFIL_*_VALIDADO para 1 em config.h.
Para uma placa nova, copie um perfil em profile_manager.c e ajuste pontos, enderecos e limites.

## Telas que a bancada NAO gera (modulo externo obrigatorio)
| Tipo | Por que o ESP8266 nao faz | Solucao modular |
|---|---|---|
| HDMI | sem gerador de video nem TMDS | gerador de padroes externo (mini PC, Raspberry Pi ou gerador HDMI) + inspecao visual |
| LVDS | sem serializador diferencial | placa controladora de LCD com saida LVDS e fonte do backlight; a bancada so liga/mede a alimentacao |
| eDP | link de alta velocidade com AUX | placa controladora eDP ou adaptador; a bancada mede alimentacao e backlight |
| MIPI DSI | sinais D-PHY de alta velocidade | placa/computador com saida DSI compativel com o painel |
| SPI TFT (ILI9341, ST7789...) | bit-bang a ~150 kHz e lento para quadros inteiros e as sequencias de init variam | exige driver proprio por controlador (nao incluido) |
Para cada familia, crie um adaptador (fiacao) e um perfil proprio, respeitando pinagem, tensao e temporizacao do datasheet do painel.

## Compilar e gravar (resumo; detalhes nos READMEs das etapas)
    cd ~/esp/auto_test_final && make menuconfig && make -j4 flash monitor
Painel: Wi-Fi AutoTest-ESP (senha autotest123) -> http://192.168.4.1/
Relatorio: botao "abrir" no historico ou http://192.168.4.1/relatorio

## O QUE NAO FOI VERIFICADO (leia antes de confiar)
- Nada foi compilado com o SDK real nem testado em hardware: so checagem de sintaxe com cabecalhos simulados.
  Nomes/assinaturas de APIs do SDK (driver/i2c.h, driver/adc.h, rom/ets_sys.h, nvs, portENTER_CRITICAL)
  podem exigir ajuste na primeira compilacao.
- Todas as constantes de calibracao (CAL_*, MUX_OFFSET_OHM, CONT_*, A0_FUNDO_ESCALA_MV, UART_US_POR_AMOSTRA)
  sao pontos de partida e PRECISAM ser medidas com instrumentos.
- O detector de baud UART e experimental (desabilita interrupcoes por ~7 ms por vez).
- Valores de ID de exemplo (INA219 0x399F, W25Q 0xEF, MPU6050/BME280 nos comentarios) devem ser
  conferidos nos datasheets dos SEUS componentes.
- Corte por software leva ~200 ms; a protecao rapida e o limite de corrente da fonte de bancada.
- Historico guarda os ultimos 8 diagnosticos; so o mais recente tem detalhes por teste no relatorio.
- Sem RTC: a data vem do navegador ao abrir o painel.
