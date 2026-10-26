Versão modificada do [Genesis Plus GX 1.7.5](https://github.com/ekeeke/Genesis-Plus-GX) (Eke-Eke) para Wii, com molduras em PNG, suporte a controles USB sem fio, Auto Crop, overclock automático e configurações por jogo. *English below.*

## Novidades desta versão

**Molduras em PNG** (Options > Video > **Bezel**), para preencher o espaço vazio ao redor do jogo numa TV LCD:
- Aparecem nos modos de imagem **INTERLACED** (480i) e **PROGRESSIVE** (480p). Em **ORIGINAL** (240p) a imagem fica como sempre, sem moldura.
- O emulador acha sozinho a área transparente no meio do PNG e encaixa a tela do jogo nela. A moldura é desenhada por cima, então cantos arredondados ficam certinhos. O Auto Crop continua funcionando dentro da moldura.
- Um PNG sem área transparente no meio vira fundo, com o jogo na frente no tamanho de sempre.
- PNG de qualquer tamanho, por exemplo os de 1920x1080 feitos para o RetroArch. A moldura cobre a imagem inteira da TV; com o Wii em 16:9, use molduras de 16:9.
- Opções: **OFF**, **DEFAULT** (a moldura de TV que já vem no emulador), **AUTO** (`<nome do jogo>.png`, senão `default.png`, senão a moldura de TV) e cada PNG da pasta `genplus/bezels` (criada quando o emulador abre), no SD ou no pendrive.
- Vem ligada na **DEFAULT**. A escolha também entra nas configurações por jogo.
- Com uma moldura que tem área transparente, a posição e a escala da tela são ignoradas: quem define o lugar do jogo é a moldura.

## O que já vinha nas versões anteriores

- **Overclock automático** (Options > System > **Main 68k Auto Overclock** e **Sub 68k Auto Overclock**), para a CPU principal e a do Sega CD: o overclock só entra quando o jogo fica lento e fica desligado nas transições. Com Show FPS ligado, aparece a velocidade em uso.
- **Configurações por jogo** (botão da engrenagem **Per-Game**), no arquivo `saves/<jogo>.cfg`.
- **Auto Crop** das bordas pretas (Video > Auto Crop: FIT ou STRETCH) e **Crop Scaling** (SMOOTH ou INTEGER).

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

Genesis Plus GX 1.7.5 for Wii with PNG bezels, Xbox 360 (XInput) USB controllers, Auto Crop, automatic overclock and per-game settings.

New in this version:
- **PNG bezels** (Options > Video > Bezel), in the INTERLACED and PROGRESSIVE display modes (not in ORIGINAL 240p). The transparent window in the middle of the PNG is found by itself and the game fills it, with the bezel drawn over its edges; a PNG without a window is a background behind the game. Any PNG size works (1920x1080 RetroArch bezels included); the bezel covers the whole picture of the TV. Choices: OFF, DEFAULT (the TV frame built into the emulator, on by default), AUTO (`<game>.png`, else `default.png`, else the TV frame) and each PNG in `genplus/bezels` on the SD card or USB drive. Kept per game too. With a window, screen position and scaling are not used.

Already in the previous versions: automatic 68k overclock (main and Sega CD), per-game settings, Auto Crop of black borders, and USB controllers read as GameCube controllers (tested with the Retroflag Classic Controller-M wireless and a generic SNES Classic Mini wireless pad, up to 4, hot-plug). Default layout for the 6-button Retroflag: A, B, C, X, Y, Z, Mode and Start as labelled; L and R also give C. **Mode + D-pad right** opens the menu.

Install: extract `GenesisPlusGX-Wii.zip` to the root of the SD card or USB drive, or replace `apps/genplus/boot.dol` in an existing install.

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke. It may be used and shared for free, but not for commercial purposes; see [LICENSE.txt](https://github.com/VrepliroidV/Genesis-Plus-GX/blob/master/LICENSE.txt), which is also in the .zip. The source code of this build is in the "Source code" files of this release.
