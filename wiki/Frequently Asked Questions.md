----

** _The emulator does not start and hangs on a black screen when loaded on my Wii._ **

Preferably use the Homebrew Channel to launch this program on your Wii, using the provided meta.xml file.

If you installed the emulator as a standalone channel or are using a different loader, you might have modified or deleted required system files (IOS), which could prevent the application from starting. Also note that some loaders are not compatible with compressed dols, instead of provided boot.dol, use genplus_wii.dol, which is not compressed. 

Lastly, if you have unsupported devices connected to the Game Cube ports or USB ports, you might want to remove them. 

----

** _The emulator loads fine but randomly freeze (no input response) or crashes (DSI exception screen) in the menu or during game._ **

Use the latest version of Homebrew Channel to launch the emulator. 

----

** _When I try to load game files from my SD card or USB drive, no files are found or an error message occurs._ **

Make sure you have at least one FAT (FAT16 or FAT32) partition on your device.

----

** _My USB2 compatible drive does not seem to be detected, is that normal ?_ **

USB2 compatibility requires IOS58 to be installed in your system (it usually came with recent System Menu updates). You must also make sure the Homebrew Channel is using this IOS (reinstall HBC if it doesn't) to load the emulator.

Please note that you cannot use both USB ports at the same time.

----

** _I can not load game files from the DVD, an error message occurs saying it can not be mounted or no valid DVD was found._ **

First, make sure the DVD has been formatted in the expected format (ISO9960/Joliet). Then, your console must be able to read burned DVD: on a non-chipped Wii, you should use the Homebrew Channel and the provided meta.xml file, which enables full access to the DVD drive, without requiring a modchip.

Please note that on most recent Wii models, it is impossible to enable DVD access anymore.

----

** _Well, I can access my device but I can't see all my game files, what is happening ?_ **

Because of memory limitations, the emulator only lists up to 1000 game files per directory. This should be more than enough for anybody but if you have a larger "collection", it is highly advised to arrange your files within subdirectories, which also increase the speed and the usability of the file browser.

----

** _When I load a game, nothing happen. I can return to the emulator menu but the game does not play or make the emulator crash._ **

You probably tried to load an unsupported game file. The emulator only supports valid Genesis / Mega Drive (.bin, .gen, .md, .smd, .mdx), Master System (.sms), Game Gear (.gg), SG-1000 (.sg) ROM files or Sega/Mega CD image files (.bin, .iso, .cue). ROM files (not CD image files !) can be zipped but make sure there is only one file per .zip archive. Also note that .rar or .7z files are NOT supported.

Lastly, make sure you are only using verified dumps: bad dumps, hacked or fixed image files are subject to crash, even on real hardware, so just get ride of them. 

----

** _I'm trying to load a CD game but I got an error message telling me the file is too big_ **

This happen if the file you are trying to load is not recognized as a valid Sega / Mega CD image. Please make sure you are only loading valid .iso or .bin files (or a .cue file pointing to a valid .iso / .bin file). Compressed image (such as .rar, .zip, .7z, etc) are NOT supported for CD emulation, you must decompress them first. Also make sure the game you are trying to load is really a Sega / Mega CD game (you wouldn't believe the amount of people trying to load Saturn games !)
 
----

** _I can hear the sound of the game I loaded but nothing is displayed but a black or distorted screen._ **

You are most likely using invalid video settings. Try deleting the configuration file (config.ini) and restart the emulator. If that does not help, try changing the following options, Display and TV Mode, and see if that fixes the issue.

----

** _I try to load a CD game but there is no audio at all, only sound effects most of the time._ **

It means you did not load a CD image with valid CD-DA tracks informations. You can confirm that by entering the CD Boot menu when resetting the game (press A to enter Boot menu) and seeing the number of tracks is stuck to 99 (max). Keep in mind that if you are using an ISO file with external audio files, only 44100hz stereo 16-bit WAV or OGG audio files are supported and you must either name them accordingly or use an associated CUE file. When using a BIN file, an associated CUE file is needed if you want to hear audio tracks. Have a look in the README for more details about supported CD image & audio tracks format and how you should load them. Also note that many games will lock or crash at some points if the expected number and lengths of audio tracks is not respected.

----

** _Well, I can hear audio tracks and the CD Boot menu show them all but the sound is buggy/scratched/distorted._ **

It is most likely due to audio files themselves. Play them on a computer with another media player to check if there are correct. Many CD rips you can find online are either corrupt or incomplete, you might want to use verified good disc dumps.

----

** _When playing CD games, I randomly experiment some slowdowns or short freezes with audio and/or video._ **

Sega/Mega CD emulation is quite CPU consuming and all in all, there is only short time allowed to the emulator to read data from the disc image. Since disc images are continuously accessed from the loading device (SD, USB, DVD), slow or inconsistent device accesses will likely cause bottlenecks in emulator processing. There is no much solutions beside using faster devices and, for USB drives, to make sure IOS58 is loaded when the emulator starts and USB2 mode is enabled.

----

** _How can I use my Classic Controller, Nunchuk, etc ? It is connected to my Wii Remote but it does not respond in games._ **

You must first configure the type of controller you want to use for each player. Go to controller settings, pick one of the player and change the affected device. Don't forget to change it back if you disconnect the expansion controller.

----

** _How can I play with the gun in Snatcher ? I successfully calibrated it using the wiimote when the game is started but it does not aim correctly during the game_ **

The calibration routine in Snatcher is kinda bugged, or more exactly, it worked with a real gun because the original Justifiers were relatively imprecise so you wouldn't really notice if it doesn't really aim the center of the screen. The solution is simply not to use the calibration option AT ALL, the default configuration works fine in-game and aims perfectly where the wiimote is pointing.

----

** _My second Wii remote controller is not detected by the emulator. How can i play 2 players games ?_ **

Any additional Wii remote must be permanently registered in your Wii system settings, otherwise it cannot be detected by homebrew applications. Go back to the Wii system menu and register each Wii remote using the red SYNC buttons, as explained in your Wii user manual.

----

** _I'm trying to use ??? feature and have put all the required files as described in the User Manual but it doesn't seem to work. What am I doing wrong ?_ **

Make sure you disable the "Hide Known Extension" feature in your OS then verify again if the required files have the proper name and extension.

----

** _How can I play 32X games in this emulator ?_ **

You can't. This program currently only emulates SG-1000, Master System, Game Gear, Genesis / Mega Drive & Sega/Mega CD hardware, including existing backward compatibility modes. Other systems might be added in the future but it's actually not high priority.

----

** _Hey, I have a very original idea. What about adding a cool feature like (insert anything) ? I think the emulator would be perfect with that feature_. **

You can always make an enhancement request in the Issue page on this site. However, if something has not been implemented yet, it has very likely a good reason not to be. Some stuff need time to work on, while others are simply no interest to me. As a general rule, I prefer focusing myself on the emulation core rather than features only designed to make the emulator interface "cooler" or "easier" to use.

----