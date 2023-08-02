/* this is based in part on main68.c from KISS-BIOS 
 * Portions Copyright (C) 2011-2016 John R. Coffman.
 * Portions Copyright (C) 2015 William R. Sowerbutts
 * Portions Copyright (C) 2023 William R. Sowerbutts
 */

#include <q40types.h>
#include <stdlib.h>
#include "q40uart.h"
#include "q40hw.h"
#include "ff.h"
#include "elf.h"
#include "bootinfo.h"
#include "tinyalloc.h"
#include "net.h"

#define MACHINE_IS_Q40

#ifdef MACHINE_IS_KISS68030
#define EXECUTABLE_LOAD_ADDRESS 0
#define MACH_THIS MACH_KISS68030
#define THIS_BOOTI_VERSION KISS68030_BOOTI_VERSION
#define CPU_THIS CPU_68030
#define MMU_THIS MMU_68030
#define FPU_THIS 0 /* no FPU */
#endif

#ifdef MACHINE_IS_Q40
#define EXECUTABLE_LOAD_ADDRESS (256*1024)
#define MACH_THIS MACH_Q40
#define THIS_BOOTI_VERSION Q40_BOOTI_VERSION
#define CPU_THIS CPU_68040
#define MMU_THIS MMU_68040
#define FPU_THIS FPU_68040
#endif

#define MAXARG 40
#define LINELEN 1024
char cmd_buffer[LINELEN];

typedef struct
{
    const char *name;
    const int min_args;
    const int max_args;
    void (* function)(char *argv[], int argc);
    const char *helpme;
} cmd_entry_t;

static void do_cd(char *argv[], int argc);
static void do_ls(char *argv[], int argc);
static void do_dump(char *argv[], int argc);
static void help(char *argv[], int argc);
static void do_writemem(char *argv[], int argc);
static void do_heapinfo(char *argv[], int argc);
static void do_netinfo(char *argv[], int argc);
static void handle_any_command(char *argv[], int argc);

const cmd_entry_t cmd_table[] = {
    /* name         min max function */
    {"cd",          1,  1,  &do_cd,	     "change directory <dir>"},
    {"dir",         0,  1,  &do_ls,	     "list directory [<vol>:]"	},
    {"dump",        2,  2,  &do_dump,	     "dump memory <from> <count>" },
    {"dm",          2,  2,  &do_dump,	     "synonym for DUMP"	},
    {"help",	    0,  0,  &help,	     "list this help info"	},
    {"ls",          0,  1,  &do_ls,	     "synonym for DIR"	},
    {"writemem",    2,  0,  &do_writemem,    "write memory <addr> [byte ...]" },
    {"wm",          2,  0,  &do_writemem,    "synonym for WRITEMEM"},
    {"heapinfo",    0,  0,  &do_heapinfo,    "info on internal malloc state" },
    {"netinfo",     0,  0,  &do_netinfo,     "network statistics" },
    {0, 0, 0, 0, 0 } /* terminator */
};

static const char * const fatfs_errmsg[] = 
{
    /* 0  */ "Succeeded",
    /* 1  */ "A hard error occurred in the low level disk I/O layer",
    /* 2  */ "Assertion failed",
    /* 3  */ "The physical drive is not operational",
    /* 4  */ "Could not find the file",
    /* 5  */ "Could not find the path",
    /* 6  */ "The path name format is invalid",
    /* 7  */ "Access denied due to prohibited access or directory full",
    /* 8  */ "Access denied due to prohibited access",
    /* 9  */ "The file/directory object is invalid",
    /* 10 */ "The physical drive is write protected",
    /* 11 */ "The logical drive number is invalid",
    /* 12 */ "The volume has no work area",
    /* 13 */ "There is no valid FAT volume",
    /* 14 */ "The f_mkfs() aborted due to any parameter error",
    /* 15 */ "Could not get a grant to access the volume within defined period",
    /* 16 */ "The operation is rejected according to the file sharing policy",
    /* 17 */ "LFN working buffer could not be allocated",
    /* 18 */ "Number of open files > _FS_LOCK",
    /* 19 */ "Given parameter is invalid"
};

static void f_perror(int errno)
{
    if(errno >= 0 && errno <= 19)
        printf("Error: %s\n", fatfs_errmsg[errno]);
    else
        printf("Error: Unknown error %d!\n", errno);
}

void pretty_dump_memory(void *start, int len)
{
    int i, rem;
    unsigned char *ptr=(unsigned char *)start;
    char linebuffer[17], *lbptr;

    for(i=0;i<16;i++)
        linebuffer[i] = ' ';
    linebuffer[16]=0;
    lbptr = &linebuffer[0];

    printf("%08x ", (unsigned)ptr&(~15));
    for(i=0; i<((unsigned)ptr & 15); i++){
        printf("   ");
        lbptr++;
    }
    while(len){
        if(*ptr >= 32 && *ptr < 127)
            *lbptr = *ptr;
        else
            *lbptr = '.';

        printf(" %02x", *ptr++);
        len--;
        lbptr++;

        if((unsigned)ptr % 16 == 0){
            printf("  %s", linebuffer);
            lbptr = &linebuffer[0];
            if(len)
                printf("\n%08x ", (int)ptr);
            else{
                /* no ragged end to tidy up! */
                printf("\n");
                return;
            }
        }
    }

    rem = 16 - ((unsigned)ptr & 15);

    for(i=0; i<rem; i++){
        printf("   ");
        *lbptr = ' ';
        lbptr++;
    }
    printf("  %s\n", linebuffer);
}

static void do_dump(char *argv[], int argc)
{
    unsigned long start, count;

    start = strtoul(argv[0], NULL, 16);
    count = strtoul(argv[1], NULL, 16);

    pretty_dump_memory((void*)start, count);
}

static void help(char *argv[], int argc)
{
    extern const cmd_entry_t cmd_table[];
    int i = 0;

    printf("\nBuilt-in commands:\n");
    while (cmd_table[i].name) {
        printf("%12s : %s\n", cmd_table[i].name,
                cmd_table[i].helpme);
        ++i;
    }
    printf("\nCommand syntax:  [u|s] [<builtin>|<file[.ext]>] [<args> ...]\n"
            "    <file[.ext]> may be *.CMD *.OUT *.68K *.SYS *.ELF  (all with Magic ID)\n\n");
}

static int fromhex(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    if(c >= 'A' && c <= 'F')
        return 10 + c - 'A';
    return -1;
}

static void do_heapinfo(char *argv[], int argc)
{
    printf("internal heap (tinyalloc):\nfree: %ld\nused: %ld, fresh: %ld\n",
            ta_num_free(), ta_num_used(), ta_num_fresh());
    printf("ta_check %s\n", ta_check() ? "ok" : "FAILED");
}

static void do_netinfo(char *argv[], int argc)
{
    int prefixlen = 0;
    uint32_t mask = interface_subnet_mask;
    while(mask){
        prefixlen++;
        mask <<= 1;
    }

    printf("IPv4 address: %d.%d.%d.%d/%d\n", 
            (int)(interface_ipv4_address >> 24 & 0xff),
            (int)(interface_ipv4_address >> 16 & 0xff),
            (int)(interface_ipv4_address >>  8 & 0xff),
            (int)(interface_ipv4_address       & 0xff),
            prefixlen);
    printf("Gateway: %d.%d.%d.%d\n", 
            (int)(interface_gateway >> 24 & 0xff),
            (int)(interface_gateway >> 16 & 0xff),
            (int)(interface_gateway >>  8 & 0xff),
            (int)(interface_gateway       & 0xff));
    printf("DNS server: %d.%d.%d.%d\n", 
            (int)(interface_dns_server >> 24 & 0xff),
            (int)(interface_dns_server >> 16 & 0xff),
            (int)(interface_dns_server >>  8 & 0xff),
            (int)(interface_dns_server       & 0xff));

    printf("packet_rx_count %ld\n", packet_rx_count);
    printf("packet_tx_count %ld\n", packet_tx_count);
    printf("packet_alive_count %ld\n", packet_alive_count);
    printf("packet_discard_count %ld\n", packet_discard_count);
    printf("packet_bad_cksum_count %ld\n", packet_bad_cksum_count);

    net_dump_packet_sinks();
}

static void do_writemem(char *argv[], int argc)
{
    unsigned long value;
    unsigned char *ptr;
    int i, j, l;

    value = strtoul(argv[0], NULL, 16);
    ptr = (unsigned char*)value;

    /* This can deal with values like: 1, 12, 1234, 123456, 12345678.
       Values > 2 characters are interpreted as big-endian words ie
       "12345678" is the same as "12 34 56 78" */

    /* first check we're happy with the arguments */
    for(i=1; i<argc; i++){
        l = strlen(argv[i]);
        if(l != 1 && l % 2){
            printf("Ambiguous value: \"%s\" (odd length).\n", argv[i]);
            return; /* abort! */
        }
        for(j=0; j<l; j++)
            if(fromhex(argv[i][j]) < 0){
                printf("Bad hex character \"%c\" in value \"%s\".\n", argv[i][j], argv[i]);
                return; /* abort! */
            }
    }

    /* then we do the write */
    for(i=1; i<argc; i++){
        l = strlen(argv[i]);
        if(l <= 2) /* one or two characters - a single byte */
            *(ptr++) = strtoul(argv[i], NULL, 16);
        else{
            /* it's a multi-byte value */
            j=0;
            while(j<l){
                value = (fromhex(argv[i][j]) << 4) | fromhex(argv[i][j+1]);
                *(ptr++) = (unsigned char)value;
                j += 2;
            }
        }
    }
}

static void do_cd(char *argv[], int argc)
{
    FRESULT r;

    r = f_chdir(argv[0]);
    if(r != FR_OK)
        f_perror(r);
}

static void select_working_drive(void)
{
    char path[4];
    DIR fat_dir;

#if FF_VOLUMES > 10
#pragma error this code needs rewriting for FF_VOLUMES greater than 10
#endif

    for(int d=0; d<FF_VOLUMES; d++){
        path[0] = '0' + d;
        path[1] = ':';
        path[2] = 0;
        f_chdrive(path);
        if(f_opendir(&fat_dir, "") == FR_OK){
            f_closedir(&fat_dir);
            return;
        }
    }

    printf("No FAT filesystem found.\n");
}

static void do_ls(char *argv[], int argc)
{
    FRESULT fr;
    const char *path, *filename;
    DIR fat_dir;
    FILINFO fat_file;
    bool dir, left = true;
    int i;

    if(argc == 0)
        path = "";
    else
        path = argv[0];

    fr = f_opendir(&fat_dir, path);
    if(fr != FR_OK){
        printf("f_opendir(\"%s\"): ", path);
        f_perror(fr);
        return;
    }

    while(1){
        fr = f_readdir(&fat_dir, &fat_file);
        if(fr != FR_OK){
            printf("f_readdir(): ");
            f_perror(fr);
            break;
        }
        if(fat_file.fname[0] == 0) /* end of directory? */
            break;
        filename = 
#if _USE_LFN
            *fat_file.lfname ? fat_file.lfname : 
#endif
            fat_file.fname;

        dir = fat_file.fattrib & AM_DIR;

        if(dir){
            /* directory */
            printf("         %04d-%02d-%02d %02d:%02d %s/", 
                    1980 + ((fat_file.fdate >> 9) & 0x7F),
                    (fat_file.fdate >> 5) & 0xF,
                    fat_file.fdate & 0x1F,
                    fat_file.ftime >> 11,
                    (fat_file.ftime >> 5) & 0x3F,
                    filename);
            for(i=strlen(fat_file.fname); i<12; i++)
                printf(" ");
        }else{
            /* regular file */
            printf("%8lu %04d-%02d-%02d %02d:%02d %-12s", 
                    fat_file.fsize, 
                    1980 + ((fat_file.fdate >> 9) & 0x7F),
                    (fat_file.fdate >> 5) & 0xF,
                    fat_file.fdate & 0x1F,
                    fat_file.ftime >> 11,
                    (fat_file.ftime >> 5) & 0x3F,
                    filename);
        }

        if(!left)
            printf("\n");
        else if(!dir)
            printf("  ");
        else
            printf(" ");
        left = !left;
    }

    if(!left)
        printf("\n");

    fr = f_closedir(&fat_dir);
    if(fr != FR_OK){
        printf("f_closedir(): ");
        f_perror(fr);
        return;
    }
}

static bool handle_cmd_builtin(char *arg[], int numarg)
{
    FRESULT fr;
    const cmd_entry_t *cmd;

    if(numarg == 1 && arg[0][strlen(arg[0])-1] == ':'){
        /* change drive */
        fr = f_chdrive(arg[0]);
        if(fr) {
            f_perror(fr);
            printf("Cannot use drive %s\n", arg[0]);
        }
        return true;
    } else {
        /* built-in command */
        for(cmd = cmd_table; cmd->name; cmd++){
            if(!strcasecmp(arg[0], cmd->name)){
                if((numarg-1) >= cmd->min_args && 
                        (cmd->max_args == 0 || (numarg-1) <= cmd->max_args)){
                    cmd->function(arg+1, numarg-1);
                }else{
                    if(cmd->min_args == cmd->max_args){
                        printf("%s: takes exactly %d argument%s\n", arg[0], cmd->min_args, cmd->min_args == 1 ? "" : "s");
                    }else{
                        printf("%s: takes %d to %d arguments\n", arg[0], cmd->min_args, cmd->max_args);
                    }
                }
                return true;
            }
        }
    }
    return false;
}

static bool load_m68k_executable(char *argv[], int argc, FIL *fd)
{
    unsigned int bytes_read;
    void *load_address = (void*)(1*1024*1024); // 1MB
    FRESULT fr;

    f_lseek(fd, 0);
    printf("Loading %lu bytes from file offset 0x%x to memory at 0x%x\n", f_size(fd), 0, (int)load_address);
    fr = f_read(fd, (void*)load_address, f_size(fd), &bytes_read);
    if(fr != FR_OK){
        printf("%s: Cannot load: ", argv[0]);
        f_perror(fr);
        return false;
    }

    void (*entry)(void) = (void(*)(void))load_address;
    printf("Loaded %d bytes. Entry at 0x%lx in supervisor mode\n", bytes_read, (long)load_address);
    q40_led(false);
    cpu_cache_flush(); // cpu_cache_flush();
    entry();
        
    return true;
}

static bool load_elf_executable(char *arg[], int numarg, FIL *fd)
{
    int i, proghead_num;
    unsigned int bytes_read;
    unsigned int highest=0;
    unsigned int lowest=~0;
    elf32_header header;
    elf32_program_header proghead;
    struct bootversion *bootver;
    struct bi_record *bootinfo;
    struct mem_info *meminfo;
    bool loaded = false;

    f_lseek(fd, 0);
    if(f_read(fd, &header, sizeof(header), &bytes_read) != FR_OK || bytes_read != sizeof(header)){
        printf("Cannot read ELF file header\n");
        return false;
    }

    if(header.ident_magic[0] != 0x7F ||
       header.ident_magic[1] != 'E' ||
       header.ident_magic[2] != 'L' ||
       header.ident_magic[3] != 'F' ||
       header.ident_version != 1){
        printf("Bad ELF header\n");
        return false;
    }

    if(header.ident_class != 1 || /* 32-bit */
       header.ident_data != 2 ||  /* big-endian */
       header.ident_osabi != 0 ||
       header.ident_abiversion != 0){
        printf("Not a 32-bit ELF file.\n");
        return false;
    }

    if(header.type != 2){
        printf("ELF file is not an executable.\n");
        return false;
    }

    if(header.machine != 4){
        printf("ELF file is not for 68000 processor.\n");
        return false;
    }

    for(proghead_num=0; proghead_num < header.phnum; proghead_num++){
        f_lseek(fd, header.phoff + proghead_num * header.phentsize);
        if(f_read(fd, &proghead, sizeof(proghead), &bytes_read) != FR_OK || bytes_read != sizeof(proghead)){
            printf("Cannot read ELF program header.\n");
            return false;
        }
        switch(proghead.type){
            case PT_NULL:
            case PT_NOTE:
            case PT_PHDR:
                break;
            case PT_SHLIB: /* "reserved but has unspecified semantics" */
            case PT_DYNAMIC:
                printf("ELF executable is dynamically linked.\n");
                return false;
            case PT_LOAD:
                if(proghead.paddr == 0){
                    /* newer linkers include the header in the first segment. fixup. */
                    proghead.offset += 0x1000;
                    proghead.paddr += 0x1000;
                    proghead.filesz -= 0x1000;
                    proghead.memsz -= 0x1000;
                }
                proghead.paddr += EXECUTABLE_LOAD_ADDRESS;
                printf("Loading %lu bytes from file offset 0x%lx to memory at 0x%lx\n", proghead.filesz, proghead.offset, proghead.paddr);
                f_lseek(fd, proghead.offset);
                if(f_read(fd, (char*)proghead.paddr, proghead.filesz, &bytes_read) != FR_OK || 
                        bytes_read != proghead.filesz){
                    printf("Unable to read segment from ELF file.\n");
                    return false;
                }
                if(proghead.memsz > proghead.filesz)
                    memset((char*)proghead.paddr + proghead.filesz, 0, 
                            proghead.memsz - proghead.filesz);
                if(proghead.paddr < lowest)
                    lowest = proghead.paddr;
                if(proghead.paddr + proghead.filesz > highest)
                    highest = proghead.paddr + proghead.filesz;
                loaded = true;
                break;
            case PT_INTERP:
                printf("ELF executable requires an interpreter.\n");
                return false;
        }
    }

    if(loaded){
        /* check for linux kernel */
        bootver = (struct bootversion*)lowest;
        if(bootver->magic == BOOTINFOV_MAGIC){
            printf("Linux kernel detected:");

            /* check machine type is supported by this kernel */
            i=0;
            while(true){
                if(!bootver->machversions[i].machtype){
                    printf(" does not support Q40/Q60.\n");
                    return false;
                }
                if(bootver->machversions[i].machtype == MACH_THIS){
                    if(bootver->machversions[i].version == THIS_BOOTI_VERSION){
                        break; /* phew */
                    }else{
                        printf(" wrong bootinfo version.\n");
                        return false;
                    }
                }
                i++; /* next machversion */
            }

            /* now we write a linux bootinfo structure at the start of the 4K page following the kernel image */
            bootinfo = (struct bi_record*)((highest + 0xfff) & ~0xfff);

            printf(" creating bootinfo at 0x%x\n", (int)bootinfo);

            /* machine type */
            bootinfo->tag = BI_MACHTYPE;
            bootinfo->data[0] = MACH_THIS;
            bootinfo->size = sizeof(struct bi_record) + sizeof(long);
            bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);

            /* CPU type */
            bootinfo->tag = BI_CPUTYPE;
            bootinfo->data[0] = CPU_THIS;
            bootinfo->size = sizeof(struct bi_record) + sizeof(long);
            bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);

            /* MMU type */
            bootinfo->tag = BI_MMUTYPE;
            bootinfo->data[0] = MMU_THIS;
            bootinfo->size = sizeof(struct bi_record) + sizeof(long);
            bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);

            /* FPU type */
            bootinfo->tag = BI_FPUTYPE;
            bootinfo->data[0] = FPU_THIS;
            bootinfo->size = sizeof(struct bi_record) + sizeof(long);
            bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);

            /* RAM location and size */
            bootinfo->tag = BI_MEMCHUNK;
            bootinfo->size = sizeof(struct bi_record) + sizeof(struct mem_info);
            meminfo = (struct mem_info*)bootinfo->data;
#ifdef MACHINE_IS_KISS68030
            meminfo->addr = 0;
            meminfo->size = (unsigned long)h_m_a;
#endif
#ifdef MACHINE_IS_Q40
            // we need to make sure our RAM starts on a multiple of 256KB it seems
            meminfo->addr = (unsigned long)EXECUTABLE_LOAD_ADDRESS;
            if(ram_size > 32*1024*1024){
                printf("WARNING: load_elf_executable needs modification to support >32MB RAM on Q40\n");
                printf("WARNING: limiting BI_MEMCHUNK to 32MB\n");
                meminfo->size = (unsigned long)(32*1024*1024 - EXECUTABLE_LOAD_ADDRESS);
            }else
                meminfo->size = (unsigned long)(ram_size - EXECUTABLE_LOAD_ADDRESS);
#endif
            bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);

            /* Now let's process the user-provided command line */
#define MAXCMDLEN 200
            const char *initrd_name = NULL;
            char kernel_cmdline[MAXCMDLEN+1];
            kernel_cmdline[0] = 0;

            for(i=1; i<numarg; i++){
                if(!strncasecmp(arg[i], "initrd=", 7)){
                    initrd_name = &arg[i][7];
                }else{
                    if(kernel_cmdline[0])
                        strncat(kernel_cmdline, " ", MAXCMDLEN);
                    strncat(kernel_cmdline, arg[i], MAXCMDLEN);
                }
            }

            /* Command line */
            i = strlen(kernel_cmdline) + 1;
            i = (i+3) & ~3; /* pad to 32-bit boundary */
            bootinfo->tag = BI_COMMAND_LINE;
            bootinfo->size = sizeof(struct bi_record) + i;
            memcpy(bootinfo->data, kernel_cmdline, i);
            bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);

            /* check for initrd */
            FIL initrd;
            if(initrd_name && (f_open(&initrd, initrd_name, FA_READ) == FR_OK)){
                bootinfo->tag = BI_RAMDISK;
                bootinfo->size = sizeof(struct bi_record) + sizeof(struct mem_info);
                meminfo = (struct mem_info*)bootinfo->data;
                /* we need to locate the initrd some distance above the kernel -- 1MB should be enough? */
                meminfo->addr = ((((unsigned long)bootinfo) + 0xfff) & ~0xfff) + 0x100000;
                meminfo->size = f_size(&initrd);
                printf("Loading initrd \"%s\": %ld bytes at 0x%lx\n", initrd_name, meminfo->size, meminfo->addr);
                if(f_read(&initrd, (char*)meminfo->addr, meminfo->size, &bytes_read) != FR_OK || 
                        bytes_read != meminfo->size){
                    printf("Unable to load initrd.\n");
                    /* if loading initrd fails, we replace this bootinfo record with BI_LAST */
                }else{
                    bootinfo = (struct bi_record*)(((char*)bootinfo) + bootinfo->size);
                }
                f_close(&initrd);
            }else if(initrd_name){
                printf("Unable to open \"%s\": No initrd.\n", initrd_name);
            }

            /* terminate the bootinfo structure */
            bootinfo->tag = BI_LAST;
            bootinfo->size = sizeof(struct bi_record);

            /* Linux expects us to enter with:
             * - interrupts disabled
             * - CPU cache disabled
             * - CPU in supervisor mode
             */
            eth_halt();
            cpu_cache_disable();
            cpu_interrupts_off();
            header.entry += EXECUTABLE_LOAD_ADDRESS;
        }else{
            printf("hmmm. don't have a way to handle non-linux executables yet?\n");
            /* run_program(umode, (void*)header.entry); */
            return false;
        }
        void (*entry)(void) = (void(*)(void))header.entry;
        printf("Entry at 0x%lx in supervisor mode\n", (long)entry);
        entry();
    }

    return true;
}

// static const char *exts[] = {
//     "CMD", "ELF", "OUT", "68K", "SYS", NULL
// };
// 
// bool extend_filename(char *argv[])
// {
// 	char *np, *tp;
// 	int i, j;
// 
// 	np = argv[0];
// 	if ( f_stat(np, NULL) == FR_OK ) return true;
// 	
// 	j = strlen(np);
// 	tp = np + j;
// 	while (--tp) {
// 		if (*tp == '.') break;	/* name was qualified, but not found */
// 		if (tp == np || *tp == '/' || *tp == ':' || *tp == '\\' ) {
// 				/* there was no qualification on the name */
//                         np = full_cmd_buffer; /* need a bit of extra space, wind back the pointer */
//                         strcpy(np, argv[0]);
//                         argv[0] = np;
// 			tp = np + j;	/* place for suffix tries */
// 			*tp++ = '.';	/* add the dot for the suffix */
// 			i = 0;
// 			while (exts[i]) {
// 				strcpy(tp, exts[i]);
// 				if (f_stat(np, NULL) == FR_OK) return true;
// 				++i;
// 			}
// 			*--tp = 0;  /* erase the added dot */
// 			return false;
// 		}
// 	}
// 
// 	return false;
// }


#define HEADER_EXAMINE_SIZE 16 /* number of bytes we examine to determine the file type */
const char coff_header_bytes[2] = { 0x01, 0x50 };
const char elf_header_bytes[4]  = { 0x7F, 0x45, 0x4c, 0x46 };
const char m68k_header_bytes[2] = { 0x60, 0x1a };

static bool handle_cmd_executable(char *argv[], int argc)
{
    FIL fd;
    FRESULT fr;
    char buffer[HEADER_EXAMINE_SIZE];
    unsigned int br;

    // ugh .. until I fix this you'll have to type the whole name in. sorry.
    // if(!extend_filename(argv))
    //     return false;

    fr = f_open(&fd, argv[0], FA_READ);

    if(fr == FR_NO_FILE || fr == FR_NO_PATH) /* file doesn't exist? */
        return false;

    if(fr != FR_OK){
        printf("%s: Cannot load: ", argv[0]);
        f_perror(fr);
        return true; /* we tried and failed */
    }

    printf("%s: %ld bytes, ", argv[0], f_size(&fd));

    /* below this point buffer holds file data, not the expanded file name */
    memset(buffer, 0, HEADER_EXAMINE_SIZE);

    /* sniff the first few bytes, then rewind to the start of the file */
    fr = f_read(&fd, buffer, HEADER_EXAMINE_SIZE, &br);
    f_lseek(&fd, 0);

    if(fr == FR_OK){
        if(memcmp(buffer, elf_header_bytes, sizeof(elf_header_bytes)) == 0){
            printf("ELF.\n");
            load_elf_executable(argv, argc, &fd);
        }else if(memcmp(buffer, coff_header_bytes, sizeof(coff_header_bytes)) == 0){
            printf("COFF: unsupported\n");
        }else if(memcmp(buffer, m68k_header_bytes, sizeof(m68k_header_bytes)) == 0){
            printf("68K or SYS\n");
            load_m68k_executable(argv, argc, &fd);
        }else{
            printf("unknown format.\n");
        }
    }else{
        printf("%s: Cannot read: ", argv[0]);
        f_perror(fr);
    }

    f_close(&fd);

    return true;
}

static void execute_cmd(char *linebuffer)
{
    char *p, *arg[MAXARG+1];
    int numarg;

    /* parse linebuffer into list of args */
    numarg = 0;
    p = linebuffer;
    while(true){
        if(numarg == MAXARG){
            printf("Limiting to %d arguments.\n", numarg);
            *p = 0;
        }
        if(!*p){ /* end of string? */
            arg[numarg] = 0;
            break;
        }
        while(isspace(*p))
            p++;
        if(!isspace(*p)){
            arg[numarg++] = p;
            while(*p && !isspace(*p))
                p++;
            if(!*p)
                continue;
            while(isspace(*p)){
                *p=0;
                p++;
            }
        }
    }

    handle_any_command(arg, numarg);
}

static void handle_any_command(char *argv[], int argc) {
    if(argc > 0){
        if(!handle_cmd_builtin(argv, argc) && !handle_cmd_executable(argv, argc))
            printf("%s: Unknown command.  Try 'help'.\n", argv[0]);
    }
}

static void rubout(void)
{
    putch('\b');
    putch(' ');
    putch('\b');
}

static int getline(char *line, int linesize)
{
    int k = 0;
    int ch;

    do {
        net_pump(); /* call this regularly */
        ch = uart_read_byte();

        if(ch >= 0){
            if (ch >= 32 && ch < 127) {
                line[k++] = ch;
                putch(ch);
            } else if (ch == '\r' || ch == '\n') {
                ch = 0;
                putch('\n');
            } else if ( (ch == '\b' || ch == 0177) && k>0) {
                rubout();
                --k;
            } else if (ch == ('X' & 037) /* Ctrl-X */) {
                while (k) { rubout(); --k; }
            } else putch('G' & 037); /* beep! */
        }
    } while (ch && k < linesize-1);

    line[k] = 0;
    return k;
}

void command_line_interpreter(void)
{
    /* select the first drive that is actually present */
    select_working_drive();

    while(true){
        f_getcwd(cmd_buffer, LINELEN);
        printf("%s> ", cmd_buffer);
        getline(cmd_buffer, LINELEN); /* periodically calls network_pump() */
        execute_cmd(cmd_buffer);
    }
}
