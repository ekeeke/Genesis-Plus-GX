
## How to start the emulator ##

Gamecube and Wii applications usually come in the form of .dol files.

Once you have downloaded and decompressed the archive, you should get two versions of the emulator:

 * genplus_cube.dol is the application running in Gamecube mode.

 This can be loaded on a Game Cube by using various methods: see http://www.gc-linux.org/wiki/Getting_Started#Booting_Code for more details.

 * genplus_wii.dol is the application running in Wii mode.

 The easiest way to run the emulator on a Wii is to install the [Homebrew Channel](http://hbc.hackmii.com/). Once you are done, simply copy the /apps directory (included with this release) and its content to the root of your SD card or USB drive.

 There are other ways to run dol files on the Wii like building a dedicated channel or using an alternate DOL loader. Feel free to visit http://www.wiibrew.org for additional information. 

----
## How to use the emulator ##

Genesis Plus GX supports Mega Drive / Genesis, Master System, Game Gear & SG-1000 ROM files and Sega/Mega CD image files. Supported ROM files must come in the form of RAW .bin, .ison .gen, .md, .smd, .mdx, .sms, .gg & .sg files. Compressed .zip files (for ROM files only) are also supported as long as they contain a single ROM file in one of the supported format.


To play a game, you first need to load a ROM file from one of the supported devices: you can either load it from DVD, SD card or USB drive (Wii only).


There is a limit of 1000 ROM files per directory so it's strongly advised to create subdirectories. Reducing the number of ROM files per directory also improves menu interface speed and usability. 


For Sega / Mega CD emulation, original BIOS files are needed for each console region: the emulator expects BIOS ROM files to be respectively named BIOS_CD_J.bin, BIOS_CD_U.bin and BIOS_CD_E.bin and placed in /genplus/bios/ directory on the default FAT device. It should not matter what BIOS versions you are using but it is recommended, for best compatibility, to use Model 1 BIOS ROM image files.

### from SD ###

The SD card should be formatted to FAT (FAT16 or FAT32). If not found, the emulator automatically creates a directory named “/genplus” at the root of your SD card, as well as subdirectories required by the emulator to store miscellaneous files (cheat, save & screenshot files). 


By default, the emulator will look for files in the sd:/genplus/roms directory but you can place them anywhere you want, the menu keeping trace of the last accessed directory for each device.


### from USB (Wii only) ###

The USB drive should have at least one partition formatted to FAT (FAT16 or FAT32), other file systems (NTFS, EXT2, etc) are not supported. If no SD card is inserted when the emulator starts, it automatically creates a directory named “/genplus” at the root of your USB drive partition, as well as subdirectories required by the emulator to store miscellaneous files (cheat, save & screenshot files). 


By default, the emulator will look for files in the usb:/genplus/roms directory but you can place them anywhere you want, the menu keeping trace of the last accessed directory for each device and for each file types.


To use an USB2 drive, you must have IOS58 installed (it should be automatically installed with System Menu 4.3 update). You should also load the emulator through the Homebrew Channel, using the provided meta.xml file and make sure Homebrew Channel is using IOS58 as default IOS. If not, you might need to reinstall Homebrew Channel after having installed IOS58.


### from DVD ###

The DVD should be formatted using ISO9660/Joliet (refer to the user manual of your DVD Burning software for more details). The Game Cube Mini-DVD drive allows up to 1.35GB of data while the Wii DVD drive allows up to 4.7GB of data (simple-layer).


By default, the emulator will look for files at the root of your DVD but you can place them anywhere you want, the menu keeping trace of the last accessed directory for each device and for each file types.


To use DVD on a non-chipped Wii, you should load the emulator through the Homebrew Channel and use the provided meta.xml file, in order to allow full access to the DVD drive.



----
*Please report to the included User Manual for additional informations.*