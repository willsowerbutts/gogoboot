# Makefile for Q40 ROM
# Will Sowerbutts 2023-07-23

CC = m68k-linux-gnu-gcc
AS = m68k-linux-gnu-as
LD = m68k-linux-gnu-ld
LIB = m68k-linux-gnu-ar
OBJCOPY = m68k-linux-gnu-objcopy
LIBS = 

COPT = -O1 -malign-int -Wall -nostdinc -nostdlib -nolibc -march=68040 -mcpu=68040 -mtune=68040 -Wall -Werror -std=c18 -Iinclude -fdata-sections -ffunction-sections
AOPT = -m68040 -alhmsg 

ROMOBJ = boot.o except.o q40uart.o q40hw.o startup.o vectors.o q40ide.o \
	 printf.o stdlib.o strtoul.o ff.o memcpy.o memmove.o memset.o \
	 ne2000.o net.o arp.o dhcp.o packet.o ipcsum.o ipv4.o icmp.o \
	 cli.o tftp.o strdup.o tinyalloc.o ffglue.o ffunicode.o

.SUFFIXES:   .c .s .o .out .hex .bin

%.o:	%.s
	$(AS) $(AOPT) -a=$*.lst $< -o $@

%.o:	%.c
	$(CC) -c $(COPT) $< -o $@

all:	q40boot.rom

test:	all
	./sendrom /dev/ttyUSB0 57600 q40boot.rom

clean:
	rm -f q40boot.rom q40boot.map *.o *.lst *.elf *.bin

q40boot.rom:	$(ROMOBJ)
	$(LD) --gc-sections --script=q40boot.ld -z noexecstack -Map q40boot.map -o q40boot.elf $(ROMOBJ)
	$(OBJCOPY) -O binary q40boot.elf q40boot.rom
