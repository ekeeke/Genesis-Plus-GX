Versão modificada do [Genesis Plus GX 1.7.5](https://github.com/ekeeke/Genesis-Plus-GX) (Eke-Eke) para Wii, com suporte a controles USB sem fio. *English below.*

## O que tem de novo

**Controles USB do tipo Xbox 360 (XInput)**, com fio ou com receptor 2.4 GHz plugado direto no USB do Wii.
- Testado com o **Retroflag Classic Controller-M wireless** (estilo Mega Drive) e com um **controle sem fio genérico de SNES Classic Mini**. Outros controles que se apresentam como controle de Xbox 360 devem funcionar também.
- Até 4 controles ao mesmo tempo. O primeiro receptor plugado é o jogador 1, o segundo é o jogador 2.
- Pode plugar e tirar o receptor com o emulador aberto.
- O emulador enxerga esses controles como **controle de GameCube**. Nas opções de controle, escolha GameCube para a porta do jogador. Todos os botões podem ser configurados.

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

O resto do emulador não mudou.

## Instalação

1. Baixe o `GenesisPlusGX-Wii.zip` e extraia na raiz do cartão SD ou do pendrive. Ele cria a pasta `apps/genplus`.
2. Se você já tem o Genesis Plus GX instalado, basta trocar o `boot.dol` da pasta `apps/genplus`. Suas configurações e saves continuam.

## Observações

- Plugue o receptor **direto no USB do Wii**. Ligado num adaptador na entrada do Wiimote, ele não funciona e pode travar o Wii.
- Funciona junto com um HD externo plugado na outra porta USB.

---

## English

Genesis Plus GX 1.7.5 for Wii with support for **Xbox 360 (XInput) USB controllers**, wired or through a 2.4 GHz receiver plugged straight into the Wii.

- Tested with the Retroflag Classic Controller-M wireless and a generic SNES Classic Mini wireless pad. Up to 4 at once; the first receiver plugged in is player 1. Hot-plug works.
- They show up as **GameCube controllers**: pick GameCube for the player's port in the controller settings. Every button can be remapped.
- Default layout for the 6-button Retroflag: A, B, C, X, Y, Z, Mode and Start as labelled; L and R also give C. On other pads the triggers and left shoulder give C, the right shoulder Z and Select Mode. **Mode + D-pad right** opens the menu.

Install: extract `GenesisPlusGX-Wii.zip` to the root of the SD card or USB drive, or replace `apps/genplus/boot.dol` in an existing install.

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke. It may be used and shared for free, but not for commercial purposes; see [LICENSE.txt](https://github.com/VrepliroidV/Genesis-Plus-GX/blob/master/LICENSE.txt), which is also in the .zip. The source code of this build is in the "Source code" files of this release.
