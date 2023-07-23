# Makefile for Q40 ROM
# Will Sowerbutts 2023-07-23

CC = m68k-linux-gnu-gcc
AS = m68k-linux-gnu-as
LD = m68k-linux-gnu-ld
LIB = m68k-linux-gnu-ar
OBJCOPY = m68k-linux-gnu-objcopy
LIBS = 

# Common
COPT = -Os -malign-int -Wall -nostdinc -nostdlib -nolibc -m68040 -Wall -Werror -std=c18
AOPT = -m68040 -alhmsg 

ROMOBJ = startup.o boot.o

.SUFFIXES:   .c .s .o .out .hex .bin

.S.o:
	$(AS) $(AOPT) -a=$*.lst -o $*.o $*.S

.c.o:
	$(CC) -S $(COPT) $*.c
	$(AS) $(AOPT) -a=$*.lst -o $*.o $*.s

all:	q40boot.rom

test:	all
	./sendrom /dev/ttyUSB0 q40boot.rom

clean:
	rm -f q40boot.rom q40boot.map *.o *.lst *.elf *.bin

q40boot.rom:	$(ROMOBJ)
	$(LD) --script=rom.ld --strip-all -z noexecstack -Map q40boot.map -o q40boot.elf $(ROMOBJ)
	$(OBJCOPY) -O binary q40boot.elf q40boot.rom


#
# SFILES = mfpic.s ppi.s allide.s siodef.s memtest.s ns202def.s biostrap.s \
# 	error.s blinken.s cache_def.s sram_test.s size_ram.s dram_test.s
# HFILES = mytypes.h packer.h mfpic.h ns202.h dosdisk.h ide.h main68.h \
#   coff.h myide.h rtc.h io.h fdc8272.h wd37c65.h debug.h diskio.h version.h \
#   stdlib.h
# HSFILES = portab.h optab.h disasm.h
# HFFILES = ff.h ffconf.h diskio.h integer.h
# OFILES = main68.o stdlib.o serial.o rtc.o ds1302.o cprintf.o \
# 	packer.o pic202.o ns202.o ppide.o dualide.o bios8.o strtoul.o \
# 	malloc.o dualsd.o bioscall.o fdc8272.o wd37c65.o floppy.o setup.o \
# 	debug.o beetle.o disasm.o ff.o prettydump.o diskio.o uart2.o \
# 	memtest3.o crctab.o fifopipe.o
# #	zdmd.o zmodemhal.o
# CSFILES = main68.s cprintf.s packer.s ns202.s crc32.s malloc.s setup.s \
# 	rtc.s strtoul.s fdc8272.s wd37c65.s ppide2.s debug.s disasm.s \
# 	prettydump.s diskio.s ff.s
# 
# 
# 
# TESTS = test3.bin test4.bin test5.bin
# 
# TABLES = startup.sym startup.mod
# TTABLES = test5.sym test5.mod daytime.sym daytime.mod
# 
# LIBFILES = cprintf.o strtoul.o bioscall.o crt0.o ff.o
# 
# 
# 
# 
# 
# all:	$(ALL)
# 	$(COPY)
# 	
# rom:	all
# 	minipro -p $(FLASH) -w $(BURN)
# 
# 
# 
# test:	$(TESTS)          #     daytime.bin $(TTABLES)
# alles:	all test
# 
# ide:	ppide2.o
# dt:	daytime.out
# fdc:	fdc8272.o wd37c65.o floppy.o
# gccver:
# 	$(CC) -v
# 	
# rw:
# 	chmod ug+w *.?
# 
# 	
# 
# mini-512.rom:	$(ROM)/rom400.bin $(TARGET).bin
# 	cat $(TARGET).bin $(ROM)/rom400.bin >mini-512.rom
# 
# mini-128.rom:	$(ROM)/rom80.bin $(TARGET).bin
# 	cat $(TARGET).bin $(ROM)/rom80.bin >mini-128.rom
# 
# kiss-512.rom:	$(ROM)/krom464.bin $(TARGET).bin
# 	cat $(TARGET).bin $(ROM)/krom464.bin >kiss-512.rom
# 
# kiss-128.rom:	$(ROM)/krom80.bin $(TARGET).bin
# 	cat $(TARGET).bin $(ROM)/krom80.bin >kiss-128.rom
# 	
# $(P)rom64.bin:	$(ROM)/$(P)rom80.bin
# 	bin2hex -o $(P)rom80.hex $(ROM)/$(P)rom80.bin
# 	hex2bin -o $(P)rom64.bin -D 64k $(P)rom80.hex
# 
# $(P)test128.bin:	$(TARGET).bin $(P)rom64.bin
# 	cat $(TARGET).bin $(P)rom64.bin >$(P)test128.bin
# 
# 	
# stamp.h:
# 	echo "\""`date`"\"" >stamp.h
# 
# stomp.h:
# 	echo "\""`date`"\"" >stomp.h
# 
# 
# $(TARGET).out:	startup.o bios.a
# 	$(LD) $(LOPT) -s -Map $(TARGET).map -o $(TARGET).elf startup.o bios.a $(LIBS)
# 	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).out
# 
# $(TARGET).hex:	$(TARGET).out
# 	bin2hex -o $(TARGET).hex -D $(BIOSSIZE)k $(TARGET).out
# 
# $(TARGET).bin:	$(TARGET).hex		# $(TUTOR)
# 	hex2bin -R $(BIOSSIZE)k $(TARGET).hex   -o $(TARGET).bin
# 
# mylib.a:   $(LIBFILES)
# 	$(LIB) -r $*.a $(LIBFILES)
# 
# bios.a:	   $(OFILES)
# 	$(LIB) -r $*.a $(OFILES)
# 
# $(TARGET).mod:	$(TARGET).out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# $(TARGET).sym:	$(TARGET).out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# 
# test1.out:	test1.o
# 	$(LD) $(LOPT) $(LIBS) -s -Map test1.map -o test1.out test1.o
# 
# test1.bin:	test1.out
# 	bin2hex -s 0x2000 test1.out -o test1.hex
# 	hex2bin -R 8k test1.hex -o test1.bin
# 
# test1.mod:	test1.out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# test1.sym:	test1.out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# 
# 
# test2.out:	test2.o
# 	$(LD) $(LOPT) $(LIBS) -s -Map test2.map -o test2.out test2.o
# 
# test2.bin:	test2.out
# 	bin2hex -s 0x2000 test2.out -o test2.hex
# 	hex2bin -R 8k test2.hex -o test2.bin
# 
# test2.mod:	test2.out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# test2.sym:	test2.out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# 
# test3.out:	test3.o
# 	$(LD) $(LOPT) $(LIBS) -s -Map test3.map -o test3.out test3.o
# 
# test3.bin:	test3.out
# 	bin2hex -s 0x2000 test3.out -o test3.hex
# 	hex2bin -R 128k test3.hex -o test3.bin
# 
# test3.mod:	test3.out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# test3.sym:	test3.out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# 
# test4.o:	test3.s memtest2.s uart.s biostrap.s hardware.s
# 	$(AS) $(AOPT) --defsym SIZE=64 -a=$*.lst -o $*.o test3.s
# 
# test4.out:	test4.o
# 	$(LD) $(LOPT) $(LIBS) -s -Map test4.map -o test4.out test4.o
# 
# test4.bin:	test4.out
# 	bin2hex -s 0x2000 test4.out -o test4.hex
# 	hex2bin -R 128k test4.hex -o test4.bin
# 
# test4.mod:	test4.out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# test4.sym:	test4.out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# 
# test5.o:	test5.s memtest2.s uart.s biostrap.s hardware.s
# 	$(AS) $(AOPT) -a=$*.lst -o $*.o test5.s
# 
# test5.out:	test5.o
# 	$(LD) $(LOPT) $(LIBS) -s -Map test5.mapr -o test5.out test5.o
# 
# test5.elf:	test5.o
# 	$(LD) $(TOPT) $(LIBS) -s -Map test5.mape -o test5.elf test5.o
# 
# test5.bin:	test5.out
# 	bin2hex -s 0x2000 test5.out -o test5.hex
# 	hex2bin -R 128k test5.hex -o test5.bin
# 
# test5.mod:	test5.out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# test5.sym:	test5.out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# 
# #  cat $(TARGET).mod $(TARGET).sym | sed -e "s/        / /g" | sort >foo
# 
# daytime.o:	daytime.c bioscall.h
# 	$(CC) -S $(COPT) $*.c
# 	$(AS) $(AOPT) -a=$*.lst -o $*.o $*.s
# 
# daytime.out:	daytime.o mylib.a
# 	$(LD) $(UOPT) -s -Map daytime.map -o daytime.out \
# 		daytime.o mylib.a $(LIBS)
# #	bin2hex -s 0x1000 daytime.out -o daytime.hex
# #	hex2bin -R 8k daytime.hex -o daytime.bin
# 
# daytime.bin:	daytime.out
# 	bin2hex -s 0x1000 daytime.out -o daytime.hex
# 	hex2bin -R 8k daytime.hex -o daytime.bin
# 
# daytime.sym:	daytime.out
# 	grep "                " $*.map | grep -v "=" | grep -v "(" | \
# 		grep -v LONG | grep -v "                 " | sort > $*.sym
# 
# daytime.mod:	daytime.out
# 	grep ".text" $*.map | grep -v ".o)" | grep ".o" | \
# 		grep -v "set to" | sed "s/.text/     /" > $*.mod
# 
# 
# cfdisk.o:	cfdisk.c cfdisk.h mytypes.h portab.h bioscall.h ide.h packer.h
# 	$(CC) -S $(COPT) $*.c
# 	$(AS) $(AOPT) -a=$*.lst -o $*.o $*.s
# 
# cfdisk.out:	cfdisk.o n8iox.o packer.o mylib.a
# 	$(LD) $(UOPT) -s -Map cfdisk.map -o cfdisk.out \
# 		cfdisk.o n8iox.o packer.o mylib.a $(LIBS)
# 
# 
# tidy:	
# 	rm -f startup.out
# 	rm -f startup.mod
# 	rm -f startup.sym
# 	rm -f *.lst *.LST
# #	rm -f *.map
# 
# clean:	tidy
# 	rm -f $(CSFILES)
# 	rm -f *.o
# 	rm -f *.a
# 	rm -f *.hex
# 	rm -f *.bin *.BIN
# 	rm -f *.out
# 	rm -f stamp.h
# 
# spotless:	clean
# 	rm -f stomp.h
# 	rm -f *.rom
# 	rm -f *.map
# 	
# 
# ## Dependencies
# startup.o:	startup.s $(SFILES)
# serial.o:	serial.s  $(SFILES)
# setup.o:	setup.c $(HFILES) stamp.h
# uart2.o:	uart2.s	$(SFILES)
# dualide.o:	dualide.s $(SFILES)
# ppide.o:	ppide.s $(SFILES)
# dualsd.o:	dualsd.s $(SFILES)
# pic202.o:	pic202.s $(SFILES)
# bios8.o:	bios8.s $(SFILES)
# bioscall.o:	bioscall.s $(SFILES)
# test5.o:	test5.s biostrap.s
# ds1302.o:	ds1302.s $(SFILES)
# floppy.o:	floppy.s $(SFILES)
# #memtest.o:	memtest.s $(SFILES)	Used on an 'include' in startup.s
# memtest3.o:	memtest3.s
# 
# foo.o:		foo.c
# time.o:		time.c $(HFILES)
# crt0.o:		crt0.s
# 
# beetle.o:	beetle.s
# n8iox.o:	n8iox.s
# 
# 
# 
