Núcleo **Genesis Plus GX** para o **RetroArch**, com as novidades da versão de Wii deste fork: **Auto Crop** das bordas pretas e **overclock automático** da CPU do Mega Drive e da CPU do Sega CD. *English below.*

## Novidades

**Overclock automático** (Quick Menu > Core Options > Hacks):
- **CPU Speed** ganhou **Auto (up to 200%)** e **Auto (up to 300%)**: o overclock da CPU do Mega Drive / Genesis só entra quando o jogo fica lento, em três passos até o limite escolhido, e volta a descer quando a cena alivia. O núcleo mede, a cada quadro, quanto tempo a CPU passa esperando.
- **Sega CD CPU Speed** (nova): 100%, 150%, 200%, 300% ou os mesmos dois modos Auto, para a CPU do Sega CD.
- O overclock fica desligado nas transições: imagem ou interrupção de vídeo desligada, paleta toda preta e envio de dados para a CPU de som. Cada jogo começa sem overclock.
- **Show Auto Overclock**: mostra no canto de cima da imagem a velocidade em uso e quanto do quadro a CPU passou trabalhando, por exemplo `OC 2.1x 72%`, e `SUB` para a CPU do Sega CD.
- Correção: com overclock, os jogos que esperam o próximo quadro com a instrução STOP perdiam um quadro a cada espera.

**Auto Crop Black Borders** (Quick Menu > Core Options > Video): corta as bordas pretas ao redor da imagem, com as mesmas regras do núcleo [snes9x-ntsc-lie-pal](https://github.com/VrepliroidV/snes9x-ntsc-lie-pal).
- **Fit (keep proportions)** amplia sem deformar os pixels; **Stretch (fill frame)** estica até preencher o quadro original.
- Linhas com a tela desligada e as bordas da opção **Borders** são cortadas em 2 quadros. Barras de tiles pretos só são cortadas quando a tela parece gameplay.
- Enquadramentos que ficam 5 segundos na tela são memorizados para o jogo, no arquivo `<jogo>.autocrop` da pasta de saves.
- As pistolas de luz acompanham o corte.
- No RetroArch, deixe **Aspect Ratio** em **Core provided**.

## Arquivos

Um ZIP por plataforma; use apenas o da sua:

| Plataforma | Núcleo |
| --- | --- |
| Windows x64 | `genesis_plus_gx_libretro.dll` |
| Linux x64 | `genesis_plus_gx_libretro.so` |
| Linux ARM64 / ROCKNIX | `genesis_plus_gx_libretro.so` |

Feche o RetroArch e faça backup do Genesis Plus GX instalado. Copie o núcleo para a pasta de núcleos do RetroArch, substituindo o original, e carregue o núcleo e o jogo de novo.

---

## English

Genesis Plus GX core for RetroArch with the features of this fork's Wii build:

- **Automatic overclock** (Core Options > Hacks): **CPU Speed** has **Auto (up to 200%)** and **Auto (up to 300%)**, and the new **Sega CD CPU Speed** has the same plus 150/200/300%. The overclock goes up in three steps only while the game slows down, and stays off while the display or the vertical interrupt is off, the palette is all black or data goes to the sound CPU. **Show Auto Overclock** draws the speed in use at the top-left of the picture. Fixed: with an overclock, games waiting with STOP lost a frame each time.
- **Auto Crop Black Borders** (Core Options > Video): Fit or Stretch, with the rules of the snes9x-ntsc-lie-pal core; layouts seen for 5 seconds are remembered in `<game>.autocrop` in the save folder. Set Aspect Ratio to Core provided.

Files: one ZIP for Windows x64 (`.dll`), Linux x64 and Linux ARM64 / ROCKNIX (`.so`).

---

Genesis Plus GX is © Charles MacDonald and Eke-Eke, libretro port © Daniel De Matteis. It may be used and shared for free, but not for commercial purposes; see [LICENSE.txt](https://github.com/VrepliroidV/Genesis-Plus-GX/blob/master/LICENSE.txt), which is also in each ZIP. The source code of this build is in the "Source code" files of this release.
