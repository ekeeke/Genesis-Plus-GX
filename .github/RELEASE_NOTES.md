Versão modificada do [Genesis Plus GX 1.7.5](https://github.com/ekeeke/Genesis-Plus-GX) (Eke-Eke) para Wii, com suporte a controles USB sem fio, Auto Crop, overclock automático e configurações por jogo. *English below.*

## Novidades desta versão

**Overclock automático** (Options > System), para a CPU principal e para a CPU do Sega CD:
- **Main 68k Auto Overclock** e **Sub 68k Auto Overclock**: o overclock só entra quando o jogo fica lento, em três passos, e volta a descer quando a cena alivia. O emulador mede, a cada quadro, quanto tempo cada CPU passa esperando.
- Com o automático ligado, **Main 68k Highest OC** e **Sub 68k Highest OC** definem o máximo de cada CPU. Com 3.0x, por exemplo, os passos são 1.44x, 2.08x e 3.0x.
- Fica desligado nas transições: imagem ou interrupção de vídeo desligada, paleta toda preta e envio de dados para o chip de som. Cada jogo começa sem overclock.
- Com **Show FPS** ligado, aparece embaixo do FPS a velocidade em uso e quanto do quadro a CPU passou trabalhando, por exemplo `OC 2.1x 72%`, e `SUB` para a CPU do Sega CD.
- Correção: com overclock, os jogos que esperam o próximo quadro com a instrução STOP perdiam um quadro a cada espera.

**Configurações por jogo** (Options > botão da engrenagem **Per-Game**): guarda as opções de vídeo, som e sistema só para o jogo aberto, no arquivo `saves/<jogo>.cfg`. Desligar o botão faz o jogo voltar às configurações gerais.

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

Genesis Plus GX 1.7.5 for Wii with Xbox 360 (XInput) USB controllers, Auto Crop, automatic overclock and per-game settings.

New in this version:
- **Automatic 68k overclock** (Options > System > Main 68k / Sub 68k Auto Overclock), for the main CPU and the Sega CD CPU: the overclock goes up in three steps, up to the Highest OC ratio, only when the game slows down, and back down when it is not needed. It stays off while the display or the vertical interrupt is off, the palette is all black or data goes to the sound chip, and each game starts without it. With Show FPS on, the ratio in use and how busy the CPU was are shown under the FPS. Fixed: with an overclock, games waiting for the next frame with STOP lost a frame each time.
- **Per-game settings** (Options > gear button): the video, audio and system options are kept for the loaded game only, in `saves/<game>.cfg`.
- **Auto Crop** (Options > Video): crops black borders, with the rules of the snes9x-ntsc-lie-pal Libretro core. OFF, FIT (keeps the pixel shape) or STRETCH (fills the frame). Layouts seen for 5 seconds are remembered per game in `saves/<system>/<game>.autocrop`; light gun crosshairs follow the crop. **Crop Scaling**: SMOOTH fills the frame and stays sharp with the bilinear filter off; INTEGER only scales by whole numbers. Settings are kept when updating (the official build would reset them).

Already in the previous version: USB controllers read as GameCube controllers (tested with the Retroflag Classic Controller-M wireless and a generic SNES Classic Mini wireless pad, up to 4, hot-plug). Default layout for the 6-button Retroflag: A, B, C, X, Y, Z, Mode and Start as labelled; L and R also give C. **Mode + D-pad right** opens the menu.

Install: extract `GenesisPlusGX-Wii.zip` to the root of the SD card or USB drive, or replace `apps/genplus/boot.dol` in an existing install.

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke. It may be used and shared for free, but not for commercial purposes; see [LICENSE.txt](https://github.com/VrepliroidV/Genesis-Plus-GX/blob/master/LICENSE.txt), which is also in the .zip. The source code of this build is in the "Source code" files of this release.
