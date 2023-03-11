
Genesis Plus GX is able to accurately emulate various console hardware models, which makes it compatible with 100% of SG-1000, Master System, Game Gear, Genesis / Mega Drive  & Sega/Mega CD released software, including demos, unlicensed & pirate games.

This means that all these games have been verified to be working without any graphical glitches or lock-up but off course, it is impossible for me to test all games *from start to end* so feel free to report an [issue](https://bitbucket.org/eke/genesis-plus-gx/issues?status=new&status=open) if you found a specific game not running accurately with this emulator.

On a side note, make sure you are always using good dumps: ROM files labeled as bad, "fixed" or hacked dumps (b1, f1, h1, ...), are known to cause issues, even on real hardware.

## Sega/Mega CD games ##

Sega/Mega CD games are only compatible with Genesis / Mega Drive hardware. Sega/Mega CD add-on is automatically enabled when a valid CD image file is detected. 


## Genesis / Mega Drive games ##

Genesis / Mega Drive games are only compatible with Genesis / Mega Drive hardware. Some games might require different hardware to be enabled (PICO games or games only working with CD unit attached) and are auto-detected.


## Game Gear games ##

Game Gear games are only compatible with Game Gear hardware.


## Master System games ##

Master System games are compatible with Game Gear and Genesis hardware, using backwards compatibility mode. However, the following games are known to be working only on specific hardware:

### _games requiring Master System BIOS to initialize VDP registers_ ###
 * California Games II
 * Speed Ball

### _games incompatible with Genesis VDP_ ###
 * Bart vs. The Space Mutants (palette issue)
 * Earthworm Jim (zoomed sprites issue)
 * X-Men - Mojo World (palette issue)

### _games requiring legacy VDP modes (not supported by Genesis VDP or Game Gear with custom BIOS)_ ###

 * Dr Hello
 * F-1 Spirit - The way to Formula-1
 * F16 Fighter
 * F16 Fighting Falcon
 * FA Tetris
 * Flashpoint
 * Loretta no Shouzou - Sherlock Holmes
 * Penguin Adventure
 * Street Master
 * Super Boy II
 * The Three Dragon Story
 * Wonsiin
 * Cyborg Z

### _games requiring extended VDP modes (Master System II & Game Gear only)_ ###
 * Cosmic Spacehead
 * Excellent Dizzy Collection, The [Proto]
 * Fantastic Dizzy
 * Micro Machines

### _games requiring original VDP limitation (Mark III & Master System I only)_ ###
 * Y'S (Japanese version)


## SG-1000 games ##

SG-1000 games are compatible with Mark-III and Master System hardware, using backwards compatibility mode (a custom BIOS, not available, is required on Game Gear). In order to display the correct colour palette, it is however recommended to use SG-1000 hardware.