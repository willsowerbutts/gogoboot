# Makefile for Q40 ROM
# Will Sowerbutts 2023-07-23

CC = m68k-linux-gnu-gcc
AS = m68k-linux-gnu-as
LD = m68k-linux-gnu-ld
LIB = m68k-linux-gnu-ar
OBJCOPY = m68k-linux-gnu-objcopy
LIBS = 

COPT = -Os -malign-int -Wall -nostdinc -nostdlib -nolibc -m68040 -Wall -Werror -std=c18 -Iinclude -fdata-sections -ffunction-sections
AOPT = -m68040 -alhmsg 

ROMOBJ = boot.o q40uart.o q40hw.o startup.o q40ide.o printf.o stdlib.o strtoul.o ff.o cli.o

.SUFFIXES:   .c .s .o .out .hex .bin

.s.o:
	$(AS) $(AOPT) -a=$*.lst -o $*.o $*.s

.c.o:
	$(CC) -c $(COPT) -o $*.o $*.c

all:	q40boot.rom

test:	all
	./sendrom /dev/ttyUSB0 57600 q40boot.rom

clean:
	rm -f q40boot.rom q40boot.map *.o *.lst *.elf *.bin

q40boot.rom:	$(ROMOBJ)
	$(LD) --gc-sections --script=q40boot.ld -z noexecstack -Map q40boot.map -o q40boot.elf $(ROMOBJ)
	$(OBJCOPY) -O binary q40boot.elf q40boot.rom
