# Makefile for genplus DOS
#
# (c) 1999, 2000, 2001, 2002, 2003  Charles MacDonald
# modified by Eke-Eke <eke_eke31@yahoo.fr>
#
# Defines :
# -DLSB_FIRST - Leave undefined for big-endian processors.
# -DDOS       - Enable DOS/x86 specific code.
# -DDEBUG     - Enable debugging code
# -DX86_ASM   - Enable inline x86 assembly code in Z80 emulator

CC	=	gcc 
AS	=	nasm -f coff
LDFLAGS	=	
FLAGS	=	-I. -I.. -I../z80 -I../m68k -I../dos -I../sound -I../sound/SRC -I../cart_hw -I../cart_hw/svp  \
		    -Wall -g \
		    -O6 -march=i686 -fomit-frame-pointer \
		    -DLSB_FIRST -DX86_ASM -DDOS

LIBS	=	-lalleg -laudio -lz -lm

OBJ	=   obj/z80.o	

OBJ	+=  obj/m68kcpu.o	\
		obj/m68kops.o	


OBJ	+=  obj/genesis.o	\
		obj/vdp.o	\
		obj/render.o	\
		obj/system.o    \
		obj/gen_io.o	\
		obj/gen_input.o	\
		obj/mem68k.o	\
		obj/memz80.o	\
		obj/membnk.o	\
		obj/memvdp.o	\
		obj/state.o     
		
OBJ	+=  obj/sound.o	\
		obj/fm.o	\
		obj/sn76489.o   \
		obj/ym2612.o    


OBJ	+=	obj/samplerate.o \
		obj/src_linear.o \
		obj/src_sinc.o   \
		obj/src_zoh.o    \

OBJ	+=  obj/sram.o      \
		obj/eeprom.o    \
		obj/svp.o	\
		obj/ssp16.o	\
		obj/cart_hw.o	

OBJ	+=	obj/dos.o	\
		obj/sealintf.o	\
		obj/config.o	\
		obj/error.o	\
		obj/unzip.o     \
		obj/fileio.o	\
		obj/loadrom.o	

EXE	=	../gen.exe

all	:	$(EXE)

$(EXE)	:	$(OBJ)
		$(CC) -o $(EXE) $(OBJ) $(LIBS) $(LDFLAGS)

obj/%.o : 	../%.c ../%.h
		$(CC) -c $< -o $@ $(FLAGS)
	        
obj/%.o :	../asm/%.s
		$(AS) $< -o $@
	        
obj/%.o :	../sound/%.c ../sound/%.h	        
		$(CC) -c $< -o $@ $(FLAGS)
obj/%.o :	../sound/%.c      
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	../sound/SRC/%.c ../sound/SRC/%.h	        
		$(CC) -c $< -o $@ $(FLAGS)
obj/%.o :	../sound/SRC/%.c      
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	../cart_hw/%.c ../cart_hw/%.h	        
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	../cart_hw/svp/%.c      
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	../cart_hw/svp/%.c ../cart_hw/svp/%.h	        
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	../z80/%.c ../z80/%.h	        
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	../m68k/%.c ../m68k/%.h	        
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	./%.c ./%.h	        
		$(CC) -c $< -o $@ $(FLAGS)
	        
pack	:
		strip $(EXE)
		upx -1 $(EXE)	        

clean	:	        
		rm -f obj/*.o
		rm -f *.bak
		rm -f *.exe
		rm -f *.log
		rm -f *.wav
		rm -f *.zip
cleancpu :		
		rm -f obj/z80.oa

makedir :
		mkdir obj
	        
archive:	        
		pk -dir -add -max \
		-excl=rom -excl=test \
		mdsrc.zip *.*
	        
#
# end of makefile
#
