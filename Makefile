# Makefile for GOGOBOOT ROM
# Will Sowerbutts 2023-07-23

CC = m68k-linux-gnu-gcc
AS = m68k-linux-gnu-as
LD = m68k-linux-gnu-ld
LIB = m68k-linux-gnu-ar
OBJCOPY = m68k-linux-gnu-objcopy

# List targets here
TARGETS = q40 kiss mini

# options for all targets
AOPT_all = -alhmsg
COPT_all = -O1 -std=c18 -Wall -Werror -malign-int -nostdinc -nostdlib -nolibc \
	   -fdata-sections -ffunction-sections -Iinclude
SRC_all = arp.c cli.c dhcp.c except.c fatfs/ff.c fatfs/ffunicode.c ffglue.c \
           ipcsum.c ipv4.c memcpy.c memmove.c memset.c ne2000.c net.c packet.c \
           printf.c qsort.c stdlib.c strdup.c strtoul.c tftp.c tinyalloc.c \
	   icmp.c ide.c timer.c mem.c uart.c boot.c

# gcc needs some helpers on 68000, system provided libgcc.a may be
# built for 68020+
SRC_68000 = libgcc/divmod.c libgcc/udivmod.c libgcc/udivmodsi4.c libgcc/mulsi3.s

# q40 target (Q40.de)
AOPT_q40 = -mcpu=68040 --defsym TARGET_Q40=1
COPT_q40 = -mcpu=68040 -DTARGET_Q40
SRC_q40 = q40/startup.s q40/vectors.s q40/cli.c q40/hw.c q40/ide.c \
	  q40/ffrtc.c q40/qdos.s q40/softrom.s

# kiss target (Retrobrew Computers KISS-68030)
AOPT_kiss = -mcpu=68030 --defsym TARGET_KISS=1
COPT_kiss = -mcpu=68030 -DTARGET_KISS
SRC_kiss = kiss/startup.s kiss/vectors.s ecb/timer.c kiss/cli.c \
	   kiss/hw.c ecb/ppide.c ecb/ffrtc.c ecb/ppidexfer.s kiss/double.s \
	   kiss/softrom.s

# mini target (Retrobrew Computers Mini68K)
AOPT_mini = -mcpu=68000 --defsym TARGET_MINI=1
COPT_mini = -mcpu=68000 -DTARGET_MINI
LDOPT_mini = --require-defined=vector_table
SRC_mini = mini/startup.s mini/vectors.s $(SRC_68000) mini/cli.c mini/hw.c \
	   ecb/timer.c ecb/ppide.c ecb/ffrtc.c ecb/ppidexfer.s

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
	$$(LD) --gc-sections --script=$(1)/linker.ld -z noexecstack --no-warn-rwx-segment -Map gogoboot-$(1).map -o gogoboot-$(1).elf $$(ROMOBJ_$(1)) version.$(1).o $$(LDOPT_$(1))

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

kiss-serial:	gogoboot-kiss.rom
	./q40/sendrom /dev/ttyUSB0 115200 gogoboot-kiss.rom

q40-split:	gogoboot-q40.rom
	./q40/makesplitrom gogoboot-q40.rom gogoboot-q40-hi.rom gogoboot-q40-lo.rom
