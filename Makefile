# Makefile for GOGOBOOT ROM
# Will Sowerbutts 2023-07-23

CC = m68k-linux-gnu-gcc
AS = m68k-linux-gnu-as
LD = m68k-linux-gnu-ld
LIB = m68k-linux-gnu-ar
OBJCOPY = m68k-linux-gnu-objcopy

# List targets here
TARGETS   = q40 kiss

AOPT_all  = -alhmsg 
COPT_all  = -O1 -std=c18 -Wall -Werror -malign-int -nostdinc -nostdlib -nolibc \
	    -fdata-sections -ffunction-sections -Iinclude
SRC_all  = arp.c cli.c dhcp.c except.c fatfs/ff.c fatfs/ffunicode.c ffglue.c \
           ipcsum.c ipv4.c memcpy.c memmove.c memset.c ne2000.c net.c packet.c \
           printf.c qsort.c stdlib.c strdup.c strtoul.c tftp.c tinyalloc.c \
	   icmp.c timer.c

# q40 target
AOPT_q40  = -m68040
COPT_q40  = -march=68040 -mcpu=68040 -mtune=68040 -DTARGET_Q40 
SRC_q40  = q40/vectors.s q40/qdos.s q40/softrom.s q40/startup.s q40/uart.c q40/hw.c \
	   q40/ide.c q40/boot.c q40/ffrtc.c

# kiss target
AOPT_kiss = -m68030
COPT_kiss = -march=68030 -mcpu=68030 -mtune=68030 -DTARGET_KISS
SRC_kiss = kiss/uart.c kiss/startup.s kiss/vectors.s kiss/boot.c kiss/timer.c \
	   kiss/ffrtc.c kiss/ide.c kiss/idexfer.s kiss/hw.c

.SUFFIXES:   .c .s .o .out .hex .bin

TARGET_ROMS = $(foreach target,$(TARGETS),gogoboot-$(target).rom)
all:	$(TARGET_ROMS)

# these rules are expanded once for each target, with $(1) = target name
define make_target =
%.$(1).o:	%.s
	$$(AS) $$(AOPT_all) $$(AOPT_$(1)) -a=$$*.lst $$< -o $$@

%.$(1).o:	%.c
	$$(CC) -c $$(COPT_all) $$(COPT_$(1)) $$< -o $$@

ROMOBJ_$(1) = $(patsubst %.s,%.$(1).o,$(patsubst %.c,%.$(1).o,$(SRC_all) $(SRC_$(1))))

gogoboot-$(1).elf:	version.$(1).o $$(ROMOBJ_$(1)) $(1)/linker.ld
	$$(LD) --gc-sections --script=$(1)/linker.ld -z noexecstack -Map gogoboot-$(1).map -o gogoboot-$(1).elf $$(ROMOBJ_$(1)) version.$(1).o

gogoboot-$(1).rom:	gogoboot-$(1).elf
	$(OBJCOPY) -O binary gogoboot-$(1).elf gogoboot-$(1).rom

endef

$(eval $(foreach target,$(TARGETS),$(call make_target,$(target))))

clean:
	rm -f *.rom *.map *.o *.lst *.elf *.bin version.c $(foreach target,$(TARGETS),$(target)/*.lst $(ROMOBJ_$(target)))

# update our version number whenever any source file changes
version.c:	$(SRC_all) $(foreach target,$(TARGETS),$(SRC_$(target)))
	./makeversion

tftp:	$(TARGET_ROMS)
	scp -C $(TARGET_ROMS) beastie:/storage/tftp/

q40-serial:	gogoboot-q40.rom
	./q40/sendrom /dev/ttyUSB0 115200 gogoboot-q40.rom

q40-split:	gogoboot-q40.rom
	./q40/makesplitrom gogoboot-q40.rom gogoboot-q40-hi.rom gogoboot-q40-lo.rom
