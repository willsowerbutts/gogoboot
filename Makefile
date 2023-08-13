# Makefile for Q40 ROM
# Will Sowerbutts 2023-07-23

CC = m68k-linux-gnu-gcc
AS = m68k-linux-gnu-as
LD = m68k-linux-gnu-ld
LIB = m68k-linux-gnu-ar
OBJCOPY = m68k-linux-gnu-objcopy
LIBS = 

COPT = -O1 -malign-int -Wall -nostdinc -nostdlib -nolibc -Wall -Werror \
       -march=68040 -mcpu=68040 -mtune=68040 -std=c18 -Iinclude \
       -fdata-sections -ffunction-sections
AOPT = -m68040 -alhmsg 

ROMOBJ = arp.o boot.o cli.o dhcp.o except.o ff.o ffglue.o ffunicode.o icmp.o \
	 ipcsum.o ipv4.o memcpy.o memmove.o memset.o ne2000.o net.o packet.o \
	 printf.o q40hw.o q40ide.o q40softrom.o q40uart.o qsort.o startup.o \
	 stdlib.o strdup.o strtoul.o tftp.o tinyalloc.o vectors.o

.SUFFIXES:   .c .s .o .out .hex .bin

%.o:	%.s
	$(AS) $(AOPT) -a=$*.lst $< -o $@

%.o:	%.c
	$(CC) -c $(COPT) $< -o $@

all:	q40boot.rom

serial:	all
	./sendrom /dev/ttyUSB0 115200 q40boot.rom

tftp:	all
	scp -C q40boot.rom beastie:/storage/tftp/

clean:
	rm -f q40boot.rom q40boot.map *.o *.lst *.elf *.bin version.c

q40boot.rom:	$(ROMOBJ) version.o
	$(LD) --gc-sections --script=q40boot.ld -z noexecstack -Map q40boot.map -o q40boot.elf $(ROMOBJ) version.o
	$(OBJCOPY) -O binary q40boot.elf q40boot.rom

# update our version number whenever any object file changes
version.c: $(ROMOBJ)
	./makeversion
