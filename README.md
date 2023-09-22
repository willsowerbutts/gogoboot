GogoBoot
========

GogoBoot is a boot ROM for 68K machines.

GogoBoot provides a command-line interface on a serial port to control the
machine.  It provides simple scripting, including a "boot" script which is
executed automatically on boot.

It supports FAT16/FAT32 (and optionally exFAT) filesystems, with long file
names. It includes a driver for IDE disks.

It includes a simple IPv4 stack which supports DHCP and can transfer files to
and from the hard disk using TFTP over an ethernet network.

It has commands to read and write to system memory from the CLI.

It can load ELF executables, including support for booting the Linux kernel.

It is mostly written in C with just enough assembler to make it work.  C was
chosen to make the software easy to port and extend.

The main purpose of GogoBoot is to provide an environment to load and run
software under development, typically operating system kernels. It is not a
operating system and does not provide a system call (BIOS) interface for
software to use at run time.

GogoBoot is in active development (as of September 2023) and may be in a broken
or incomplete state. 

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

I use a Debian (12) Linux system with the gcc-m68k-linux-gnu cross-compiler
package. Some of the scripts use `python`.

Just type `make -j` and you should end up with a `.rom` and `.elf` file for
each target.

To program EPROMs for the Q40, run `make q40-split` and separate high/low
`.rom` files will be generated.


CLI
---

The `help` command lists the various commands available. There is little or
no documentation beyond this at the moment...

The `hardrom` command will reboot the machine using whatever is in the
hardware ROMs.

The `softrom` command is very similar to the SMSQ/E command of the same name,
it reprograms the "LowRAM" in the Q40 using a ROM image and then boots from
it. You can run `softrom <filename>` to load a ROM image from the FAT
partition, or if you run it without a filename it will try to load an image
over the UART. The format on the wire is identical to the SMSQ/E `softrom`
command except that it runs at 115,200 bps.  The `q40/sendrom` file in the
source is a python script that will send a ROM image in the required format.

Before executing the ROM, softrom compares it to what is already running. If
it matches, softrom does NOT reboot the machine.

The `tftpget` (or `tftp`) command will download a file from a TFTP server, and
`tftpput` will upload a file to a TFPT server. The syntax is:

    tftpget 1.2.3.4 sourcefile diskfile
    tftpput 1.2.3.4 sourcefile destfile

Where `1.2.3.4` is the TFTP server IP address, `sourcefile` is the filename
on the TFTP server, and `destfile` is the target filename on the local disk.

You can also run `set tftp_server 1.2.3.4` to set the `tftp_server` environment
variable. Thereafter you can omit the server IP, and optionally also the
destination filename, from the command line (ie `tftp somefile` will work,
using the same filename for the source and destination).

If you put a text file on the FAT partition starting with `#!script` then
this is treated as a batch file. If you have a file in the root of the
partition named `boot` it will be executed automatically. 

My own `boot` script looks like this:

    #!script
    softrom gogoboot-q40.rom
    loadimage images/boys.img
    set tftp_server "192.168.100.80"

When the version in ROM boots, it runs the `softrom` line which loads the
latest version from disk and boots into that instead. This saves me from
needing to reprogram the EPROMs regularly. When it reboots using the new
version, this line runs again but this time the image matches what is running
so we do not reboot and instead just continue to the next command.

The `loadimage` command, on the Q40 target, loads video memory with a raw 1MB
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
GogoBoot/q40: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Version git c5c4068+ build 2023-09-02 carbon/1093
RAM installed: 32 MB
 segment     start    length
    text         0      AF14   43.8 KB
  rodata      AF14      1FA4    7.9 KB
    data     18000         4    0.0 KB  load from CEB8
     bss     18004       124    0.3 KB
  (free)     18128   1DE7ED8   29.9 MB
    heap   1E00000    1FE000    2.0 MB
   stack   1FFE000      2000    8.0 KB
Setup interrupts: done
Initialise RTC: Sat 2023-09-02 00:41:11
IDE controller at 0x1F0:
  Probe disk 0: InnoDisk Corp. - iCF4000 2GB (4095504 sectors, 1999 MB)
  Probe disk 1: SDCFHS-016G (31293360 sectors, 15279 MB)
IDE controller at 0x170:
  Probe disk 0: no disk found.
  Probe disk 1: no disk found.
Initialise video: done
Initialise ethernet: NE2000 at 0x300, MAC 00:00:E8:CF:2E:39
Booting from "boot" (hit Q to cancel)
DHCP lease acquired (192.168.100.191/24, 5h 58m)
boot: 179 bytes, script
boot: softrom gogoboot-q40.rom
softrom: loaded 52924 bytes from "gogoboot-q40.rom"
softrom: matches running ROM
boot: loadimage images/vaporwave.img
boot: set tftp_server "192.168.100.80"
boot: set linux_parameters "console=ttyS0,115200n8 root=/dev/sda3 netdev=5,0x300,eth0"
1:/> ls
       179 2023-08-27 22:05 boot
     52924 2023-09-02 00:41 gogoboot-q40.rom
           2023-08-12 21:10 images/
        88 2023-08-18 23:31 linux
   6013952 2023-07-25 17:58 linux.513
   6502484 2023-07-25 17:57 linux.621
     18299 2023-08-12 17:57 q40boot-old.rom
     47247 2023-08-12 17:58 q40boot.041
     47607 2023-08-12 23:19 q40boot.106
     49271 2023-08-13 01:00 q40boot.148
     49911 2023-08-14 01:10 q40boot.189
     49991 2023-08-27 22:03 q40boot.206
        56 2000-07-08 10:11 rom
    262144 2023-08-18 23:14 smsqe-297.rom
    282050 2023-07-26 11:19 smsqe-338
   6479788 2000-07-08 10:57 vmlinux
   6023848 2023-08-13 21:55 vmlinux-5.13.19
   6025404 2023-08-13 21:56 vmlinux-5.13.19+44b1f
   5979196 2023-08-13 21:56 vmlinux-5.14.21
   6482976 2023-08-13 22:09 vmlinux-6.4.10+rfc
   6482976 2023-08-14 11:15 vmlinux-6.4.10+rfc2
   6509524 2023-08-14 12:04 vmlinux-6.4.10+rfc2+wrs
   6482816 2023-08-13 22:33 vmlinux-6.4.10+wrs
953 MB free (243980 clusters of 4096 bytes)
1:/> tftp vmlinux
tftp: get 192.168.100.80:vmlinux to file "vmlinux"
Transfer started: Press Q to abort
tftp: server using port 16868
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
Transferred 6479788 bytes in 6.9s (7.51 Mbit/sec)
1:/> vmlinux console=ttyS0,115200n8 root=/dev/sda3 netdev=5,0x300,eth0
vmlinux: 6479788 bytes, ELF.
Load address range 0x0 -- 0x4AC000
Load would overlap ROM, offsetting by 0x40000
Load address range 0x40000 -- 0x4EC000
Loading 0x46D4C0 bytes + 0x20354 padding from file offset 0x0 to memory at 0x40000
Loading 0x1D37C bytes + 0xC84 padding from file offset 0x46E000 to memory at 0x4CE000
Linux kernel detected: creating bootinfo at 0x4EC000
Entry at 0x42000 in supervisor mode, SP 0x2000000
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
[    0.000000] Console: colour dummy device 80x25
[    0.010000] printk: console [ttyS0] enabled
[    0.090000] Calibrating delay loop... 26.16 BogoMIPS (lpj=130816)
[    0.200000] pid_max: default: 32768 minimum: 301
[    0.210000] Mount-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.220000] Mountpoint-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.260000] cblist_init_generic: Setting adjustable number of callback queues.
[    0.260000] cblist_init_generic: Setting shift to 0 and lim to 1.
[    0.280000] devtmpfs: initialized
[    0.310000] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 19112604462750000 ns
[    0.310000] futex hash table entries: 256 (order: -1, 3072 bytes, linear)
[    0.350000] NET: Registered PF_NETLINK/PF_ROUTE protocol family
[    0.350000] DMA: preallocated 128 KiB GFP_KERNEL pool for atomic allocations
[    0.360000] DMA: preallocated 128 KiB GFP_KERNEL|GFP_DMA pool for atomic allocations
[    0.420000] SCSI subsystem initialized
[    0.780000] NET: Registered PF_INET protocol family
[    0.790000] IP idents hash table entries: 2048 (order: 2, 16384 bytes, linear)
[    0.820000] tcp_listen_portaddr_hash hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.830000] Table-perturb hash table entries: 65536 (order: 6, 262144 bytes, linear)
[    0.830000] TCP established hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.840000] TCP bind hash table entries: 1024 (order: 1, 8192 bytes, linear)
[    0.850000] TCP: Hash tables configured (established 1024 bind 1024)
[    0.860000] UDP hash table entries: 256 (order: 0, 4096 bytes, linear)
[    0.870000] UDP-Lite hash table entries: 256 (order: 0, 4096 bytes, linear)
[    0.880000] NET: Registered PF_UNIX/PF_LOCAL protocol family
[    0.900000] workingset: timestamp_bits=30 max_order=13 bucket_order=0
[    1.300000] fb0: Q40 frame buffer alive and kicking !
[    1.310000] Serial: 8250/16550 driver, 8 ports, IRQ sharing disabled
[    1.320000] serial8250: ttyS0 at I/O 0x3f8 (irq = 4, base_baud = 115200) is a 16450
[    1.330000] serial8250: ttyS1 at I/O 0x2f8 (irq = 3, base_baud = 115200) is a 16450
[    1.540000] loop: module loaded
[    1.550000] atari-falcon-ide atari-falcon-ide.0: Atari Falcon and Q40/Q60 PATA controller
[    1.570000] scsi host0: pata_falcon
[    1.580000] ata1: PATA max PIO4 cmd 000001f0 ctl 000003f6 data ff4007c0 irq 14
[    1.590000] atari-falcon-ide atari-falcon-ide.1: Atari Falcon and Q40/Q60 PATA controller
[    1.620000] scsi host1: pata_falcon
[    1.630000] ata2: PATA max PIO4 cmd 00000170 ctl 00000376 data ff4005c0 irq 15
[    1.700000] ne ne.0 (unnamed net_device) (uninitialized): NE*000 ethercard probe at 0x300:
[    1.710000] 00:00:e8:cf:2e:39
[    1.730000] ne ne.0 eth0: RTL8019 found at 0x300, using IRQ 5.
[    1.740000] PPP generic driver version 2.4.2
[    1.760000] serio: Q40 kbd registered
[    1.780000] NET: Registered PF_INET6 protocol family
[    1.810000] input: AT Raw Set 2 keyboard as /devices/platform/q40kbd/serio0/input/input0
[    1.840000] ata1.00: CFA: InnoDisk Corp. - iCF4000 2GB, 080414S4, max UDMA/66
[    1.840000] ata1.00: 4095504 sectors, multi 2: LBA
[    1.850000] ata1.01: CFA: SDCFHS-016G, HDX13.04, max UDMA/100
[    1.850000] ata1.01: 31293360 sectors, multi 0: LBA48
[    1.860000] ata1.00: configured for PIO
[    1.860000] ata1.01: configured for PIO
[    1.870000] scsi 0:0:0:0: Direct-Access     ATA      InnoDisk Corp. - 14S4 PQ: 0 ANSI: 5
[    1.910000] scsi 0:0:1:0: Direct-Access     ATA      SDCFHS-016G      3.04 PQ: 0 ANSI: 5
[    1.950000] sd 0:0:0:0: [sda] 4095504 512-byte logical blocks: (2.10 GB/1.95 GiB)
[    1.960000] sd 0:0:0:0: [sda] Write Protect is off
[    1.970000] sd 0:0:0:0: [sda] Write cache: disabled, read cache: enabled, doesn't support DPO or FUA
[    1.980000] sd 0:0:0:0: [sda] Preferred minimum I/O size 512 bytes
[    2.000000] sd 0:0:1:0: [sdb] 31293360 512-byte logical blocks: (16.0 GB/14.9 GiB)
[    2.010000] sd 0:0:1:0: [sdb] Write Protect is off
[    2.020000] sd 0:0:1:0: [sdb] Write cache: disabled, read cache: enabled, doesn't support DPO or FUA
[    2.030000] sd 0:0:1:0: [sdb] Preferred minimum I/O size 512 bytes
[    2.060000] Segment Routing with IPv6
[    2.070000] In-situ OAM (IOAM) with IPv6
[    2.280000]  sda: AHDI sda1 sda2 sda3
[    2.310000] sd 0:0:1:0: [sdb] Attached SCSI removable disk
[    2.320000] sd 0:0:0:0: [sda] Attached SCSI disk
[    2.440000] EXT4-fs (sda3): INFO: recovery required on readonly filesystem
[    2.450000] EXT4-fs (sda3): write access will be enabled during recovery
[    2.960000] EXT4-fs (sda3): recovery complete
[    3.000000] EXT4-fs (sda3): mounted filesystem b9cf5dc9-ff50-4f58-ad15-3c19315ac42a ro with ordered data mode. Quota mode: disabled.
[    3.010000] VFS: Mounted root (ext4 filesystem) readonly on device 8:3.
[    3.030000] devtmpfs: mounted
[    3.030000] Freeing unused kernel image (initmem) memory: 120K
[    3.040000] This architecture does not have kernel memory protection.
[    3.040000] Run /sbin/init as init process
[    4.010000] EXT4-fs (sda3): re-mounted b9cf5dc9-ff50-4f58-ad15-3c19315ac42a r/w. Quota mode: disabled.
Starting syslogd: OK
Starting klogd: OK
Running sysctl: OK
Initializing random number generator: OK
Saving random seed: OK
[    7.490000] random: dd: uninitialized urandom read (32 bytes read)
Starting network: OK

Welcome to Buildroot
buildroot login: 
```
