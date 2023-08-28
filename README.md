GogoBoot
========

GogoBoot is a boot ROM for 68K machines.  It is mostly written in C with just
enough assembler to make it work.  C was chosen to make the software easy to
port and extend.

GogoBoot provides a command-line interface on a serial port to control the
machine.  It provides simple scripting, including a script executed
automatically on boot.

It supports FAT16/FAT32 (and optionally exFAT) filesystems, with long file
names, on IDE disks.

It includes a simple IPv4 stack which supports DHCP and can transfer files to
the hard disk using TFTP over an ethernet network. 

It can load ELF executables, including support for booting the Linux kernel.

The main purpose of GogoBoot is to provide an environment to load and run
software under development, typically operating system kernels. It does not
provide a system call (BIOS) interface for software to use at run time.


Supported Targets
-----------------

GogoBoot currently supports three target machines:
 - Retrobrew Computers KISS-68030
 - Retrobrew Computers Mini-68K
 - Peter Graf's Q40 Sinclair QL successor

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

