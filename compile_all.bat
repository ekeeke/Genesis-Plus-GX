make -f Makefile.wii
make -f Makefile.gc
rm *.elf
cp genplus_wii.dol boot.dol
pause
