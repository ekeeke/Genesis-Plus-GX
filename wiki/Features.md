
Genesis Plus main goal is to provide the most complete & accurate emulation of the Sega Genesis/Megadrive hardware.

The original emulation core from [Charles Mac Donald](http://cgfm2.emuviews.com/) has been largely modified to improve overall accuracy and therefore compatibility, as well as adding emulation of various peripherals, cartridge and system hardware.

![sega_megadrive_2.thumbnail.jpg](https://bitbucket.org/repo/7AjE6M/images/4007718275-sega_megadrive_2.thumbnail.jpg)


## Very Accurate & Full Speed Sega 8-bit / 16-bit emulation ##
  * accurate emulation of SG-1000, Mark-III, Master System (I & II), Game Gear, Genesis / Mega Drive, Sega / Mega CD hardware models (incl. backwards compatibility modes)
  * NTSC (60Hz) & PAL (50Hz) video hardware emulation
  * highly accurate 68000 & Z80 CPU emulation & synchronization
  * highly accurate VDP emulation (all rendering modes, mid-line changes, undocumented registers,…) & timings (HBLANK, DMA, FIFO, HV interrupts,…)
  * sample-accurate YM2612,YM2413, SN76489, & RF5C164 PCM sound chips emulation 
  * cycle-accurate sound chips synchronization with 68000/Z80 CPU
  * cycle-accurate 68000 & Z80 CPU synchronization
  * optimized Main-CPU / Sub-CPU synchronization (Sega/Mega CD)
  * accurate CDD, CDC & GFX chip emulation (Sega/Mega CD)
  * accurate CD-DA fader emulation (Sega/Mega CD)
  * Mode 1 cartridge support (Sega/Mega CD)
  * Audio CD & CD+G support (Sega/Mega CD)
  * high-quality audio resampling using Blip Buffer
  * basic hardware latency emulation (VDP/68k, Z80/68k)
  * full overscan area emulation (horizontal & vertical color borders)
  * optional Game Gear extended screen mode
  * optional Game Gear LCD ghosting filter
  * optional Blargg's NTSC filters
  * optional BOOT ROM support (Master System, Game Gear, Genesis / Mega Drive)
  * optional TMSS hardware emulation (Genesis / Mega Drive)
  * preliminary PICO emulation
  * support for raw (.bin, .gen, .md, .sms, .gg & .sg) and interleaved (.smd & .mdx) ROM files
  * support for various CD image file formats (CUE+BIN,  ISO+WAV & ISO+OGG)
  * support for subcodes external files (SUB)


![street.gif](https://bitbucket.org/repo/7AjE6M/images/607524734-street.gif)

## Support for various peripherals ##
  * 2-buttons, 3-buttons & 6-buttons Control Pads 
  * Sega Team Player & EA 4-Way Play multitaps
  * Master Tap 
  * Sega Mouse
  * Sega Paddle Control
  * Sega Sports Pad
  * Sega Graphics Board
  * Terebi Oekaki tablet
  * Sega Light Phaser
  * Sega Menacer 
  * Konami Justifiers
  * Sega Activator
  * XE-1AP analog controller


![menacer.jpg](https://bitbucket.org/repo/7AjE6M/images/4221166085-menacer.jpg)

## Support for various cartridges extra hardware ##
  * SVP DSP (Virtua Racing)
  * J-Cart adapter (Micro Machines & Pete Sampras series, Super Skidmarks)
  * Backup RAM (max. 64KB)
  * I2C (24Cxx), SPI (95xxx) & MicroWire (93C46) EEPROMs
  * RAM cart (max. 512KB) (Sega/Mega CD)
  * “official” ROM bankswitch hardware (Super Street Fighter 2)
  * “official” backup RAM bankswitch hardware (Phantasy Star 4, Legend of Thor, Sonic the Hedgehog 3)
  * all known unlicensed/pirate cartridges bankswitch & copy protection hardware
  * all known Master System & Game Gear cartridge “mappers” (incl. unlicensed Korean ones)
  * Game Genie & Action Replay hardware emulation
  * Sonic & Knuckles “Lock-On” hardware emulation
  * support for ROM image up to 10MB (Ultimate MK3 hack)

![vracing.png](https://bitbucket.org/repo/7AjE6M/images/802538951-vracing.png)

## Gamecube/Wii generic features ##

  * fully featured & optimized Graphical User Interface
  * 48 kHz stereo sound
  * optimized GX video rendering engine
  * perfect audio/video/input synchronization
  * 50/60 Hz video output support
  * original low-resolution video modes support (interlaced & non-interlaced)
  * high-resolution interlaced (480i/576i) & progressive (480p/576p) video modes support
  * hardware bilinear filtering
  * configurable BIOS & Lock-on ROM files
  * configurable sound mixer (FM/PSG levels) and filtering (Low-Pass filter & 3-Band equalizer)
  * configurable NTSC filter
  * independently configurable region mode, VDP mode & Master Clock
  * 1~4 Players support
  * automatic Backup RAM and State files loading/saving
  * automatic game files loading
  * game files loading history
  * load files from SD/SDHC or DVD
  * support for zipped ROM files
  * game internal header information display
  * internal game screenshots
  * Game Genie & Pro Action Replay codes support through .pat files
  * cartridge "hot-swap"
  * automatic disc swap


## Wii extra features ##
  * up to 8 Players support 
  * Wii Remote, Nunchuk & Classic controllers support 
  * Wii Remote IR support & calibration for light guns
  * Wii U Pro Controller support
  * USB mouse support for mouse emulation
  * USB drive support (IOS58 is required for USB2)
  * configurable hardware “Trap” filter & Gamma correction
  * "Wiiflow" plugin compatibility

![wiimote.jpg](https://bitbucket.org/repo/7AjE6M/images/2853669073-wiimote.jpg)