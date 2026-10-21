Versão modificada do [Genesis Plus GX 1.7.5](https://github.com/ekeeke/Genesis-Plus-GX) (Eke-Eke) para Wii, com suporte a controles USB sem fio e Auto Crop. *English below.*

## Novidades desta versão

**Auto Crop** (Options > Video > Auto Crop): corta as bordas pretas ao redor da imagem do jogo, com as mesmas regras do núcleo [snes9x-ntsc-lie-pal](https://github.com/VrepliroidV/snes9x-ntsc-lie-pal).
- **OFF**, **FIT** (amplia sem deformar os pixels) ou **STRETCH** (estica até preencher o quadro).
- Linhas desenhadas com a tela desligada, e as bordas da opção **Borders** quando a cor delas é preta, são cortadas em 2 quadros. Barras de tiles pretos só são cortadas quando a tela parece gameplay, então telas de logo e texto sobre fundo preto ficam inteiras.
- Enquadramentos que ficam 5 segundos na tela são memorizados para o jogo, no arquivo `saves/<sistema>/<jogo>.autocrop`. Apague o arquivo para o jogo aprender de novo.
- Gráficos que aparecem na área cortada voltam a aparecer no mesmo quadro. As miras das pistolas de luz acompanham o corte.
- **Crop Scaling**: **SMOOTH** preenche a tela e fica nítido mesmo com o filtro bilinear desligado. **INTEGER** amplia só em número inteiro, sem nenhuma linha misturada; em 240p, num jogo com faixas em cima e embaixo, a altura fica como está.

Suas configurações continuam ao atualizar. Se um dia voltar para o Genesis Plus GX original, ele vai recriar as configurações do zero.

## O que já vinha na versão anterior

**Controles USB do tipo Xbox 360 (XInput)**, com fio ou com receptor 2.4 GHz plugado direto no USB do Wii.
- Testado com o **Retroflag Classic Controller-M wireless** (estilo Mega Drive) e com um **controle sem fio genérico de SNES Classic Mini**. Até 4 controles; o primeiro receptor plugado é o jogador 1. Pode plugar e tirar com o emulador aberto.
- O emulador vê esses controles como **controle de GameCube**. Nas opções de controle, escolha GameCube para a porta do jogador. Todos os botões podem ser configurados.

Botões já configurados, pensados para o Retroflag de 6 botões:

| Retroflag | Mega Drive |
|---|---|
| A, B, C | A, B, C |
| X, Y, Z | X, Y, Z |
| L e R | C |
| Mode | Mode |
| Start | Start |
| **Mode + direcional para a direita** | abre o menu do emulador |

Em outros controles, os gatilhos e o botão de ombro esquerdo dão C, o ombro direito dá Z e Select dá Mode.

## Instalação

1. Baixe o `GenesisPlusGX-Wii.zip` e extraia na raiz do cartão SD ou do pendrive. Ele cria a pasta `apps/genplus`.
2. Se você já tem o Genesis Plus GX instalado, basta trocar o `boot.dol` da pasta `apps/genplus`. Suas configurações e saves continuam.

## Observações

- Plugue o receptor **direto no USB do Wii**. Ligado num adaptador na entrada do Wiimote, ele não funciona e pode travar o Wii.
- Funciona junto com um HD externo plugado na outra porta USB.

---

## English

Genesis Plus GX 1.7.5 for Wii with Xbox 360 (XInput) USB controllers and Auto Crop.

New in this version: **Auto Crop** (Options > Video): crops black borders, with the rules of the snes9x-ntsc-lie-pal Libretro core. OFF, FIT (keeps the pixel shape) or STRETCH (fills the frame). Layouts seen for 5 seconds are remembered per game in `saves/<system>/<game>.autocrop`; light gun crosshairs follow the crop. **Crop Scaling**: SMOOTH fills the frame and stays sharp with the bilinear filter off; INTEGER only scales by whole numbers. Settings are kept when updating (the official build would reset them).

Already in the previous version: USB controllers read as GameCube controllers (tested with the Retroflag Classic Controller-M wireless and a generic SNES Classic Mini wireless pad, up to 4, hot-plug). Default layout for the 6-button Retroflag: A, B, C, X, Y, Z, Mode and Start as labelled; L and R also give C. **Mode + D-pad right** opens the menu.

Install: extract `GenesisPlusGX-Wii.zip` to the root of the SD card or USB drive, or replace `apps/genplus/boot.dol` in an existing install.

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke. It may be used and shared for free, but not for commercial purposes; see [LICENSE.txt](https://github.com/VrepliroidV/Genesis-Plus-GX/blob/master/LICENSE.txt), which is also in the .zip. The source code of this build is in the "Source code" files of this release.
