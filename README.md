GogoBoot
========

GogoBoot is a boot ROM for 68K machines.

GogoBoot is in active development (as of 2023-08-31) and may be in a broken
state. 

GogoBoot provides a command-line interface on a serial port to control the
machine.  It provides simple scripting, including a "boot" script which is
executed automatically on boot.

It supports FAT16/FAT32 (and optionally exFAT) filesystems, with long file
names. It includes a driver for IDE disks.

It includes a simple IPv4 stack which supports DHCP and can transfer files to
the hard disk using TFTP over an ethernet network.

It has commands to read and write to system memory from the CLI.

It can load ELF executables, including support for booting the Linux kernel.

It is mostly written in C with just enough assembler to make it work.  C was
chosen to make the software easy to port and extend.

The main purpose of GogoBoot is to provide an environment to load and run
software under development, typically operating system kernels. It is not a
operating system and does not provide a system call (BIOS) interface for
software to use at run time.


Supported Targets
-----------------

GogoBoot currently supports three target machines:
 - Retrobrew Computers KISS-68030
   - 68030 CPU at 32MHz, 256MB DRAM (max), ECB expansion with MF/PIC card at 0x40
 - Retrobrew Computers Mini-68K
   - 68008 CPU at 8MHz, 2B SRAM (max), ECB expansion with MF/PIC card at 0x40
 - Peter Graf's Q40 Sinclair QL successor
   - 68040 CPU at 40MHz, 32MB DRAM (max), ISA expansion with Super-IO card

It should be easy to port GogoBoot to a new target. The Mini-68K target was
written in just a few hours.


Supported Hardware
------------------

- 68K processor (tested on 68008, 68030, 68040)
- 8250/16450/16550 UART
- IDE disk interface (direct legacy PATA port-based access, and 8255 PPIDE access)
- NE2000 network card (in 8 or 16 bit mode)
- Real time clock

GogoBoot expects IDE hard disks to have an MBR partition table, with a FAT16 or
FAT32 formatted partition.  I have my own disks partitioned with a FAT32
partition for GogoBoot, a Linux swap partition, and a Linux ext4 partition. If
you have multiple drives you can switch between them using "0:", "1:" etc
(comparable to "A:", "B:" in DOS).

It does not use interrupts for the ethernet, disk or serial -- only for the
timer. This keeps the software simple and reliable, as a boot ROM should be,
by avoiding a lot of nasty concurrency problems.

On Q40 machines, note that the disk should be in PC-style byte order, in
contrast to SMSQ/E disks which normally store data byte-swapped. This was done
to afford easy interchange of disks with other computers.

On Q40 machines, GogoBoot will look for an NE2000 ISA ethernet card at the
common I/O addresses (I use 0x300).


Building GoGoBoot
-----------------

I use a Debian (12) Linux system with the gcc-m68k-linux-gnu. Some of the
scripts use `python`.

Just type `make -j` and you will end up with a `.rom` file for each target.

To program EPROMs for the Q40, run `make q40-split` and separate high/low
`.rom` files will be generated.


CLI
---

The "help" command lists the various commands available. There is little or
no documentation beyond this at the moment...

The "hardrom" command will reboot the machine using whatever is in the
hardware ROMs.

The "softrom" command is very similar to the SMSQ/E command of the same name,
it reprograms the "LowRAM" in the Q40 using a ROM image and then boots from
it. You can run "softrom <filename>" to load a ROM image from the FAT
partition, or if you run it without a filename it will try to load an image
over the UART. The format on the wire is identical to the SMSQ/E "softrom"
command except that it runs at 115,200 bps.  The "q40/sendrom" file in the
source is a python script that will send a ROM image in the required format.

Before executing the ROM, softrom compares it to what is already running. If
it matches, softrom does NOT reboot the machine.

The "tftp" command will retrieve a file from a TFTP server. The syntax is:

    tftp 1.2.3.4 sourcefile destfile

Where "1.2.3.4" is the TFTP server IP address, "sourcefile" is the filename
on the TFTP server, and "destfile" is the target filename on the local disk.

You can also run "set tftp_server 1.2.3.4" to set the tftp_server environment
variable, then you can omit this, and optionally also the destination
filename, from the command line (ie "tftp somefile" will work, using the same
filename for the source and destination).

If you put a text file on the FAT partition starting with "#!script" then
this is treated as a batch file. If you have a file in the root of the
partition named "boot" it will be executed automatically. 

My own boot script looks like this:

    #!script
    softrom gogoboot-q40.rom
    loadimage images/boys.img
    set tftp_server "192.168.100.80"

When the version in ROM boots, it runs the "softrom" line which loads the
latest version from disk and boots into that instead. This saves me from
needing to reprogram the EPROMs regularly. When it reboots using the new
version, this line runs again but this time the image matches what is running
so we do not reboot and instead just continue to the next command.

The "loadimage" command, on the Q40 target, loads video memory with a raw 1MB
file from disk. The file is raw 1024x512x16 bit colour. I have discovered my
wife questions why I spend hours and hours working on these things, but if I
show her this feature loading a picture of our kids she suddenly seems
satisfied that the time was well spent!

To boot a Linux image you just run the vmlinux ELF file and provide the
kernel command line parameters. There is some code in there to load an
initrd, although I never use it myself so it is not well tested.

I have a second script to load a kernel image from my TFTP server and run it:

    #!script
    tftp vmlinux
    vmlinux console=ttyS0,115200n8 root=/dev/sda3 netdev=5,0x300,eth0

On Q40, you can also boot an SMSQ/E "ROM" image by naming it. So on my Q40
machine with GogoBoot in ROM I can still boot into SMSQ/E and retain the
original functionality of the machine. One unexplained thing is that while I
can load and run SMSQ/E 2.97, it does not work with 3.38 -- I haven't yet
understood why this is, but it doesn't bother me too much because I can boot
SMSQ/E 2.97 from GogoBoot and then LRESPR SMSQ/E 3.38 from inside 2.97, which
works fine.

Sample session
--------------

```
GOGOBOOT/Q40: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

  .text    0x00000000 length 0x0000A758
  .rodata  0x0000A758 length 0x00001F14
  .stack   0x0001E000 length 0x00002000
  .data    0x00018000 length 0x00000004 (load from 0x0000C66C)
  .bss     0x00018004 length 0x00000E18

Version git 1d7ad9b build 2023-08-29 carbon/817
RAM installed: 32 MB, 2 MB heap at 0x01E00000
Setup interrupts: done
Initialise RTC: Mon 2023-08-28 22:31:58
IDE controller at 0x1F0:
  Probe disk 0: InnoDisk Corp. - iCF4000 2GB (4095504 sectors, 1999 MB)
  Probe disk 1: SDCFHS-016G (31293360 sectors, 15279 MB)
IDE controller at 0x170:
  Probe disk 0: no disk found.
  Probe disk 1: no disk found.
Initialise video: done
Initialise ethernet: NE2000 at 0x300, MAC 00:00:E8:CF:2E:39
Booting from "boot" (hit Q to cancel)
DHCP lease acquired (192.168.100.191/24, 5h 59m)
boot: 179 bytes, script
boot: softrom gogoboot-q40.rom
softrom: loaded 50752 bytes from "gogoboot-q40.rom"
softrom: matches running ROM
boot: loadimage images/vaporwave.img
boot: set tftp_server "192.168.100.80"
1:/> 
1:/> ls
       179 2023-08-27 22:05 boot
     50800 2023-08-29 10:44 gogoboot-q40.rom
           2023-08-12 21:10 images/
        88 2023-08-18 23:31 linux
   6013952 2023-07-25 17:58 linux.513
   6502484 2023-07-25 17:57 linux.621
        56 2023-08-27 22:04 rom
    262144 2023-08-18 23:14 smsqe-297.rom
    282050 2023-07-26 11:19 smsqe-338
   6479788 2023-08-28 22:32 vmlinux
   6023848 2023-08-13 21:55 vmlinux-5.13.19
   6025404 2023-08-13 21:56 vmlinux-5.13.19+44b1f
   5979196 2023-08-13 21:56 vmlinux-5.14.21
   6482976 2023-08-13 22:09 vmlinux-6.4.10+rfc
   6482976 2023-08-14 11:15 vmlinux-6.4.10+rfc2
   6509524 2023-08-14 12:04 vmlinux-6.4.10+rfc2+wrs
   6482816 2023-08-13 22:33 vmlinux-6.4.10+wrs
953 MB free (243980 clusters of 4096 bytes)
1:/> linux
linux: 88 bytes, script
linux: tftp vmlinux
tftp: get 192.168.100.80:vmlinux to file "vmlinux"
Transfer started: Press Q to abort
tftp: server using port 38519
tftp: options ack: tsize=6479788 blksize=1024 rollover=0 windowsize=8
tftp: received 256/6327 KB
tftp: received 512/6327 KB
tftp: received 768/6327 KB
tftp: received 1024/6327 KB
tftp: received 1280/6327 KB
tftp: received 1536/6327 KB
tftp: received 1792/6327 KB
tftp: received 2048/6327 KB
tftp: received 2304/6327 KB
tftp: received 2560/6327 KB
tftp: received 2816/6327 KB
tftp: received 3072/6327 KB
tftp: received 3328/6327 KB
tftp: received 3584/6327 KB
tftp: received 3840/6327 KB
tftp: received 4096/6327 KB
tftp: received 4352/6327 KB
tftp: received 4608/6327 KB
tftp: received 4864/6327 KB
tftp: received 5120/6327 KB
tftp: received 5376/6327 KB
tftp: received 5632/6327 KB
tftp: received 5888/6327 KB
tftp: received 6144/6327 KB
tftp: received 6327/6327 KB
Transfer success.
Transferred 6479788 bytes in 6.7s (7.73 Mbit/sec)
linux: vmlinux console=ttyS0,115200n8 root=/dev/sda3 netdev=5,0x300,eth0
vmlinux: 6479788 bytes, ELF.
Loading 4637888 bytes from file offset 0x1000 to memory at 0x41000
Loading 119676 bytes from file offset 0x46E000 to memory at 0x4CE000
Linux kernel detected: creating bootinfo at 0x4EC000
Entry at 0x42000 in supervisor mode
[    0.000000] Linux version 6.4.10-wrs (btg@carbon) (m68k-linux-gnu-gcc (Debian 12.2.0-13) 12.2.0, GNU ld (GNU Binutils for Debian) 2.40) #1 Sat Aug 26 11:47:15 BST 2023
[    0.000000] Zone ranges:
[    0.000000]   DMA      [mem 0x0000000000040000-0x0000001fffffffff]
[    0.000000]   Normal   empty
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000000040000-0x0000000001ffffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000000040000-0x0000000001ffffff]
[    0.000000] Kernel command line: console=ttyS0,115200n8 root=/dev/sda3 netdev=5,0x300,eth0
[    0.000000] Dentry cache hash table entries: 4096 (order: 2, 16384 bytes, linear)
[    0.000000] Inode-cache hash table entries: 2048 (order: 1, 8192 bytes, linear)
[    0.000000] Sorting __ex_table...
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 8056
[    0.000000] mem auto-init: stack:off, heap alloc:off, heap free:off
[    0.000000] Memory: 27340K/32512K available (3219K kernel code, 443K rwdata, 988K rodata, 120K init, 128K bss, 5172K reserved, 0K cma-reserved)
[    0.000000] NR_IRQS: 43
[    0.010000] Console: colour dummy device 80x25
[    0.010000] printk: console [ttyS0] enabled
[    0.090000] Calibrating delay loop... 26.16 BogoMIPS (lpj=130816)
[    0.200000] pid_max: default: 32768 minimum: 301
[    0.210000] Mount-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.220000] Mountpoint-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.260000] cblist_init_generic: Setting adjustable number of callback queues.
[    0.260000] cblist_init_generic: Setting shift to 0 and lim to 1.
[    0.280000] devtmpfs: initialized
  (etc)
```
