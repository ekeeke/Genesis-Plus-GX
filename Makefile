.PHONY = all wii gc wii-clean gc-clean wii-run gc-run

all: wii gc

clean: wii-clean gc-clean

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

wii-run:
	$(MAKE) -f Makefile.wii run

gc:
	$(MAKE) -f Makefile.gc

gc-clean:
	$(MAKE) -f Makefile.gc clean

gc-run:
	$(MAKE) -f Makefile.gc run
