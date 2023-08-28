/* this is based in part on main68.c from KISS-BIOS 
 * Portions Copyright (C) 2011-2016 John R. Coffman.
 * Portions Copyright (C) 2015-2023 William R. Sowerbutts
 */

#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <q40/hw.h>
#include <fatfs/ff.h>
#include <elf.h>
#include <bootinfo.h>
#include <tinyalloc.h>
#include <net.h>
#include <cpu.h>
#include <cli.h>
#include <init.h>

#ifdef TARGET_MINI
#define EXECUTABLE_LOAD_ADDRESS (256*1024)
#endif

#ifdef TARGET_KISS
#define EXECUTABLE_LOAD_ADDRESS (256*1024)
#define MACH_THIS MACH_KISS68030
#define THIS_BOOTI_VERSION KISS68030_BOOTI_VERSION
#define CPU_THIS CPU_68030
#define MMU_THIS MMU_68030
#define FPU_THIS 0 /* no FPU */
#endif

#ifdef TARGET_Q40
#define EXECUTABLE_LOAD_ADDRESS (256*1024)
#define MACH_THIS MACH_Q40
#define THIS_BOOTI_VERSION Q40_BOOTI_VERSION
#define CPU_THIS CPU_68040
#define MMU_THIS MMU_68040
#define FPU_THIS FPU_68040
#endif

#define AUTOBOOT_FILENAME "boot"
#define AUTOBOOT_TIMEOUT_MS 500 /* this is actually enough as you can pre-stuff the UART receiver */

#define MAXARG 40
#define LINELEN 1024
char cmd_buffer[LINELEN];

typedef struct environment_variable_t environment_variable_t;

struct environment_variable_t
{
    char *name;
    char *value;
    environment_variable_t *next;
};

environment_variable_t *environment_list_head = NULL;

const char *get_environment_variable(const char *name)
{
    environment_variable_t *e;

    for(e = environment_list_head; e; e = e->next)
        if(strcasecmp(e->name, name)==0)
            return e->value;

    return NULL;
}

static void execute_cmd(char *linebuffer);
static void do_cd(char *argv[], int argc);
static void do_ls(char *argv[], int argc);
static void do_rm(char *argv[], int argc);
static void do_mkdir(char *argv[], int argc);
static void do_mv(char *argv[], int argc);
static void do_cp(char *argv[], int argc);
static void do_dump(char *argv[], int argc);
static void help(char *argv[], int argc);
static void do_writemem(char *argv[], int argc);
static void do_heapinfo(char *argv[], int argc);
static void do_netinfo(char *argv[], int argc);
static void do_set(char *argv[], int argc);
static void do_tftp(char *argv[], int argc);
static void handle_any_command(char *argv[], int argc);

static const cmd_entry_t builtin_cmd_table[] = {
    /* name         min     max function */
    {"cd",          1,      1,  &do_cd,       "change directory <dir>"},
    {"del",         1, MAXARG,  &do_rm,       "delete a file" },
    {"dir",         0,      1,  &do_ls,       "list directory [<vol>:]"  },
    {"dm",          2,      2,  &do_dump,     "synonym for DUMP" },
    {"dump",        2,      2,  &do_dump,     "dump memory <from> <count>" },
    {"heapinfo",    0,      0,  &do_heapinfo, "info on internal malloc state" },
    {"help",        0,      0,  &help,        "list this help info"   },
    {"ls",          0,      1,  &do_ls,       "synonym for DIR"  },
    {"mkdir",       1,      1,  &do_mkdir,    "make a directory" },
    {"mv",          2,      2,  &do_mv,       "rename a file" },
    {"cp",          2,      2,  &do_cp,       "copy a file" },
    {"copy",        2,      2,  &do_cp,       "copy a file" },
    {"netinfo",     0,      0,  &do_netinfo,  "network statistics" },
    {"rename",      2,      2,  &do_mv,       "rename a file" },
    {"rm",          1, MAXARG,  &do_rm,       "delete a file" },
    {"set",         0,      2,  &do_set,      "show or set environment variables" },
    {"tftp",        1,      3,  &do_tftp,     "retrieve file with TFTP" }, // TODO write down the syntax
    {"wm",          2,      0,  &do_writemem, "synonym for WRITEMEM"},
    {"writemem",    2,      0,  &do_writemem, "write memory <addr> [byte ...]" },
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

const char *f_errmsg(int errno)
{
    if(errno >= 0 && errno <= 19)
        return fatfs_errmsg[errno];
    return "???";
}

void f_perror(int errno)
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

static void help_cmd_table(const cmd_entry_t *cmd)
{
    while (cmd->name) {
        printf("%12s : %s\n", cmd->name, cmd->helpme);
        cmd++;
    }
}

static void help(char *argv[], int argc)
{
    help_cmd_table(builtin_cmd_table);
    help_cmd_table(target_cmd_table);
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
    printf("internal heap (tinyalloc):\nfresh: %ld\nfree: %ld\nused: %ld\n",
            ta_num_fresh(), ta_num_free(), ta_num_used());
    printf("ta_check %s\n", ta_check() ? "ok" : "FAILED");
}

static void do_set(char *argv[], int argc)
{
    environment_variable_t *e;

    if(argc == 0){
        // no args -- print environment
        int count = 0;
        for(e = environment_list_head; e; e = e->next){
            printf("%s=%s\n", e->name, e->value);
            count++;
        }
        printf("%d environment variables\n", count);
        return;
    }

    if(argc >= 1){
        bool found = false;
        // remove variable (we will set it again in next step if argc == 2)
        environment_variable_t **ptr;
        ptr = &environment_list_head;
        e = environment_list_head;
        while(e){
            if(strcasecmp(e->name, argv[0])==0){
                *ptr = e->next;
                free(e->name);
                free(e->value);
                free(e);
                found = true;
                break;
            }
            ptr = &e->next;
            e = e->next;
        }
        if(argc == 1 && !found)
            printf("Cannot find \"%s\" in environment\n", argv[0]);
    }

    if(argc >= 2){
        // set variable
        e = malloc(sizeof(environment_variable_t));
        e->name = strdup(argv[0]);
        e->value = strdup(argv[1]);
        // convert e->name to lower case
        for(char *p=e->name; *p; p++) 
            *p = tolower(*p);
        // add to linked list
        e->next = environment_list_head;
        environment_list_head = e;
    }
}

static void do_tftp(char *argv[], int argc)
{
    const char *server=NULL, *src, *dst;
    uint32_t targetip = 0;

    // this needs some improvements to make it more user friendly
    // right now it expects the user to know too much
    
    switch(argc){
        case 1:
            src = dst = argv[0];
            break;
        case 2:
            src = argv[0];
            dst = argv[1];
            break;
        case 3:
            server = argv[0];
            src = argv[1];
            dst = argv[2];
            break;
        default:
            printf("Unexpected number of arguments\n");
            return;
    }

    if(!server)
        server = get_environment_variable("tftp_server");

    if(!server){
        printf("please specify the server IP address (or 'set tftp_server <ip>')\n");
        return;
    }

    if(server){
        targetip = net_parse_ipv4(server);
        if(targetip == 0){
            printf("Cannot parse server IPv4 address \"%s\"\n", server);
            return;
        }
    }

    tftp_receive(targetip, src, dst);
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
            (int)(interface_ipv4_gateway >> 24 & 0xff),
            (int)(interface_ipv4_gateway >> 16 & 0xff),
            (int)(interface_ipv4_gateway >>  8 & 0xff),
            (int)(interface_ipv4_gateway       & 0xff));
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

static void do_mkdir(char *argv[], int argc)
{
    FRESULT fr = f_mkdir(argv[0]);
    if(fr != FR_OK){
        printf("f_mkdir(\"%s\"): ", argv[0]);
        f_perror(fr);
    }
}

#define COPY_BUFFER_SIZE 65536
static void do_cp(char *argv[], int argc)
{
    FRESULT fr;
    FIL src, dst;
    char *buffer;
    bool done = false;
    UINT bytes_read, bytes_written;

    fr = f_open(&src, argv[0], FA_READ);
    if(fr != FR_OK){
        printf("f_open(\"%s\"): ", argv[0]);
        f_perror(fr);
        return;
    }

    fr = f_open(&dst, argv[1], FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK){
        printf("f_open(\"%s\"): ", argv[1]);
        f_perror(fr);
        f_close(&src);
        return;
    }

    buffer = malloc(COPY_BUFFER_SIZE);
    if(!buffer){
        printf("Out of memory\n");
    }else{
        while(!done){
            fr = f_read(&src, buffer, COPY_BUFFER_SIZE, &bytes_read);
            if(fr != FR_OK){
                printf("f_read(\"%s\"): ", argv[0]);
                f_perror(fr);
                done = true;
            }else{
                if(bytes_read > 0){
                    fr = f_write(&dst, buffer, bytes_read, &bytes_written);
                    if(fr != FR_OK || bytes_read != bytes_written){
                        printf("f_write(\"%s\"): ", argv[1]);
                        f_perror(fr);
                        done = true;
                    }
                }
                if(bytes_read < COPY_BUFFER_SIZE)
                    done = true;
            }
        }
        free(buffer);
    }

    fr = f_close(&src);
    if(fr != FR_OK) f_perror(fr);
    fr = f_close(&dst);
    if(fr != FR_OK) f_perror(fr);
}

static void do_mv(char *argv[], int argc)
{
    FRESULT fr = f_rename(argv[0], argv[1]);
    if(fr != FR_OK){
        printf("f_rename(\"%s\", \"%s\"): ", argv[0], argv[1]);
        f_perror(fr);
    }
}

static void do_rm(char *argv[], int argc)
{
    FRESULT fr;
    DIR di;
    FILINFO fi;

    for(int i=0; i<argc; i++){
        while(true){
            fr = f_findfirst(&di, &fi, "", argv[i]);
            f_closedir(&di); // in any event we MUST close the dir before calling f_unlink
            if(fr == FR_OK && fi.fname[0]){
                printf("Deleting \"%s\"\n", fi.fname);
                f_unlink(fi.fname);
            }else
                break;
        }
    }
}

static int ls_sort_name(const void *a, const void *b)
{
    return strcasecmp((*(FILINFO**)a)->fname, (*(FILINFO**)b)->fname);
}

static void do_ls(char *argv[], int argc)
{
    FRESULT fr;
    const char *path;
    DIR fat_dir;
    FILINFO *fat_file;
    FILINFO **fat_file_ptr;
    DWORD free_clusters, csize, free_sectors;
    FATFS *fatfs;
    int fat_file_used = 0;
    int fat_file_length = 2;

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

    fat_file = malloc(sizeof(FILINFO) * fat_file_length);

    while(true){
        if(fat_file_used == fat_file_length){
            fat_file_length *= 2;
            fat_file = realloc(fat_file, sizeof(FILINFO) * fat_file_length);
        }
        fr = f_readdir(&fat_dir, &fat_file[fat_file_used]);
        if(fr != FR_OK){
            printf("f_readdir(): ");
            f_perror(fr);
            break;
        }
        if(fat_file[fat_file_used].fname[0] == 0) /* end of directory? */
            break;
        fat_file_used++;
    }
    fr = f_closedir(&fat_dir);

    if(fr != FR_OK){
        printf("f_closedir(): ");
        f_perror(fr);
        // report but keep going
    }

    // sort into name order
    // it is faster to sort pointers, since it avoids copying around 
    // the entries themselves, which are quite large with LFN enabled.
    fat_file_ptr = (FILINFO**)malloc(sizeof(FILINFO*)*fat_file_used);
    for(int i=0; i<fat_file_used; i++)
        fat_file_ptr[i] = &fat_file[i];

    qsort(fat_file_ptr, fat_file_used, sizeof(FILINFO*), ls_sort_name);
    
    for(int i=0; i<fat_file_used; i++){
        if(fat_file_ptr[i]->fattrib & AM_DIR){
            /* directory */
            printf("           %04d-%02d-%02d %02d:%02d %s/", 
                    1980 + ((fat_file_ptr[i]->fdate >> 9) & 0x7F),
                    (fat_file_ptr[i]->fdate >> 5) & 0xF,
                    fat_file_ptr[i]->fdate & 0x1F,
                    fat_file_ptr[i]->ftime >> 11,
                    (fat_file_ptr[i]->ftime >> 5) & 0x3F,
                    fat_file_ptr[i]->fname);
        }else{
            /* regular file */
            printf("%10lu %04d-%02d-%02d %02d:%02d %s", 
                    fat_file_ptr[i]->fsize, 
                    1980 + ((fat_file_ptr[i]->fdate >> 9) & 0x7F),
                    (fat_file_ptr[i]->fdate >> 5) & 0xF,
                    fat_file_ptr[i]->fdate & 0x1F,
                    fat_file_ptr[i]->ftime >> 11,
                    (fat_file_ptr[i]->ftime >> 5) & 0x3F,
                    fat_file_ptr[i]->fname);
        }

        printf("\n");
    }

    free(fat_file);
    free(fat_file_ptr);

    fr = f_getfree(path, &free_clusters, &fatfs);
    if(fr != FR_OK){
        printf("f_getfree(\"%s\"): ", path);
        f_perror(fr);
        return;
    }

    // calculate cluster size in bytes
    csize = 
        #if FF_MAX_SS != FF_MIN_SS
        fatfs->ssize
        #else
        FF_MAX_SS
        #endif
        * fatfs->csize;

    // free space measured in units of 512 bytes (max 2TB in 32 bits)
    free_sectors = (csize >> 9) * free_clusters;
        
    printf("%ld MB free (%ld clusters of %ld bytes)\n", free_sectors >> 11, free_clusters, csize);
}

static bool handle_cmd_drive(char *arg[], int numarg)
{
    FRESULT fr;

    if(numarg == 1 && arg[0][strlen(arg[0])-1] == ':'){
        /* change drive */
        fr = f_chdrive(arg[0]);
        if(fr) {
            f_perror(fr);
            printf("Cannot use drive %s\n", arg[0]);
        }
        return true;
    }
    return false;
}

static bool handle_cmd_table(char *arg[], int numarg, const cmd_entry_t *cmd)
{
    while(cmd->name){
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
        cmd++;
    }
    return false;
}

static bool load_m68k_executable(char *argv[], int argc, FIL *fd)
{
    unsigned int bytes_read;
    void *load_address = (void*)(4*1024*1024); // is highest or lowest best?! how to choose?
    FRESULT fr;

    f_lseek(fd, 0);
    printf("Loading %lu bytes from file offset 0x%x to memory at 0x%x\n", f_size(fd), 0, (int)load_address);
    fr = f_read(fd, (void*)load_address, f_size(fd), &bytes_read);
    if(fr != FR_OK){
        printf("%s: Cannot load: ", argv[0]);
        f_perror(fr);
        return false;
    }

    printf("Loaded %d bytes. Entry at 0x%lx in supervisor mode\n", bytes_read, (long)load_address);
    cpu_cache_flush();
#ifdef TARGET_Q40
    // q40softboot.s does a "power on reset" of the machine, in a similar way to SOFTROM
    // - CPU registers, MMU, caches reset to power-on defaults
    // - CPLD registers all zeroed to power-on defaults
    // - LowRAM mode is NOT changed, this differs from power-on, when it is always disabled
    q40_boot_qdos(load_address); 
#else
    void (*entry)(void) = (void(*)(void))load_address;
    entry();
#endif
        
    return true;
}

static bool load_elf_executable(char *arg[], int numarg, FIL *fd)
{
    int proghead_num;
    unsigned int bytes_read;
    unsigned int highest=0;
    unsigned int lowest=~0;
    elf32_header header;
    elf32_program_header proghead;
#ifdef MACH_THIS
    struct bootversion *bootver;
    struct bi_record *bootinfo;
    struct mem_info *meminfo;
#endif
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
#ifdef MACH_THIS
        /* check for linux kernel */
        bootver = (struct bootversion*)lowest;
        if(bootver->magic == BOOTINFOV_MAGIC){
            printf("Linux kernel detected:");

            /* check machine type is supported by this kernel */
            for(int i=0;; i++){
                if(!bootver->machversions[i].machtype){
                    printf(" does not support this machine type.\n");
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
#ifdef TARGET_KISS
            meminfo->addr = 0;
            meminfo->size = (unsigned long)ram_size;
#endif
#ifdef TARGET_Q40
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

            for(int i=1; i<numarg; i++){
                if(!strncasecmp(arg[i], "initrd=", 7)){
                    initrd_name = &arg[i][7];
                }else{
                    if(kernel_cmdline[0])
                        strncat(kernel_cmdline, " ", MAXCMDLEN);
                    strncat(kernel_cmdline, arg[i], MAXCMDLEN);
                }
            }

            /* Command line */
            int i = strlen(kernel_cmdline) + 1;
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
#endif
        void (*entry)(void) = (void(*)(void))header.entry;
        printf("Entry at 0x%lx in supervisor mode\n", (long)entry);
        cpu_cache_flush();
        entry();
    }

    return true;
}

static void execute_script(char *_name, FIL *fd) /* the buffer name lives in will be re-used shortly */
{
    int i;
    bool eof;
    unsigned int bytes_read;
    char name[40];

    strncpy(name, _name, sizeof(name));
    name[sizeof(name)-1] = 0; /* ensure null termination */

    eof = false;
    i = 0;
    do{
        if(f_read(fd, &cmd_buffer[i], 1, &bytes_read) != FR_OK || bytes_read != 1){
            cmd_buffer[i] = '\n';
            eof = true;
        }
        if(cmd_buffer[i] == '\n' || cmd_buffer[i] == '\r'){
            cmd_buffer[i] = 0;
            net_pump(); /* yes, once per line inside scripts! */
            if(i > 0){
                if(cmd_buffer[0] != '#'){
                    printf("%s: %s\n", name, cmd_buffer);
                    execute_cmd(cmd_buffer);
                }
            }
            i = 0;
        }else
            i++;
        if(i == LINELEN){
            printf("Script line too long!\n");
            eof = true;
        }
    }while(!eof);
}


// static const char *exts[] = {
//     "CMD", "ELF", "OUT", "68K", "SYS", NULL
// };
// 
// bool extend_filename(char *argv[])
// {
//      char *np, *tp;
//      int i, j;
// 
//      np = argv[0];
//      if ( f_stat(np, NULL) == FR_OK ) return true;
//      
//      j = strlen(np);
//      tp = np + j;
//      while (--tp) {
//              if (*tp == '.') break;  /* name was qualified, but not found */
//              if (tp == np || *tp == '/' || *tp == ':' || *tp == '\\' ) {
//                              /* there was no qualification on the name */
//                         np = full_cmd_buffer; /* need a bit of extra space, wind back the pointer */
//                         strcpy(np, argv[0]);
//                         argv[0] = np;
//                      tp = np + j;    /* place for suffix tries */
//                      *tp++ = '.';    /* add the dot for the suffix */
//                      i = 0;
//                      while (exts[i]) {
//                              strcpy(tp, exts[i]);
//                              if (f_stat(np, NULL) == FR_OK) return true;
//                              ++i;
//                      }
//                      *--tp = 0;  /* erase the added dot */
//                      return false;
//              }
//      }
// 
//      return false;
// }


#define HEADER_EXAMINE_SIZE 16 /* number of bytes we examine to determine the file type */
const char coff_header_bytes[2] = { 0x01, 0x50 };
const char elf_header_bytes[4]  = { 0x7F, 0x45, 0x4c, 0x46 };
const char m68k_header_bytes[2] = { 0x60, 0x1a };
const char script_header_bytes[8] = "#!script"; /* case insensitive */

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
        }else if(strncasecmp(buffer, script_header_bytes, sizeof(script_header_bytes)) == 0){
            printf("script\n");
            execute_script(argv[0], &fd);
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
    char *p, *arg[MAXARG+1], term;
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
        // skip whitespace before arg
        while(isspace(*p))
            p++;
        if(*p == '"' || *p == '\''){
            term = *(p++);
            arg[numarg++] = p;
            while(*p && *p != term)
                p++;
            if(!*p){
                printf("Could not find end of quoted string (looking for %c)\n", term);
                return;
            }
            *(p++) = 0;
        }else if(*p){
            arg[numarg++] = p;
            while(*p && !isspace(*p))
                p++;
            if(*p)
                *(p++) = 0;
        }
    }

    //printf("argc=%d", numarg);
    //for(int i=0; i<numarg; i++)
    //    printf(" argv[%d]=\"%s\"", i, arg[i]);
    //printf("\n");

    handle_any_command(arg, numarg);
}

static void handle_any_command(char *argv[], int argc) 
{
    if(argc == 0)
        return;

    if(!handle_cmd_drive(argv, argc) && 
       !handle_cmd_table(argv, argc, target_cmd_table) && 
       !handle_cmd_table(argv, argc, builtin_cmd_table) && 
       !handle_cmd_executable(argv, argc)) {
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

static void run_autoexec(const char *filename)
{
    FRESULT fr;
    FIL fd;
    int c;
    timer_t timer;

    fr = f_open(&fd, filename, FA_READ);
    if(fr == FR_OK){
        f_close(&fd);
    }else{
        printf("No \"%s\" script: %s\n", filename, f_errmsg(fr));
        return; // nothing found
    }

    timer = set_timer_ms(AUTOBOOT_TIMEOUT_MS);
    printf("Booting from \"%s\" (hit Q to cancel)\n", filename);
    while(!timer_expired(timer)){
        net_pump();
        c = uart_read_byte();
        if(c == 'q' || c == 'Q')
            return;
    }

    strcpy(cmd_buffer, filename);
    execute_cmd(cmd_buffer);
}

void command_line_interpreter(void)
{
    /* select the first drive that is actually present */
    select_working_drive();

    /* check for autoexec file */
    run_autoexec(AUTOBOOT_FILENAME);

    while(true){
        f_getcwd(cmd_buffer, LINELEN);
        printf("%s> ", cmd_buffer);
        getline(cmd_buffer, LINELEN); /* periodically calls network_pump() */
        execute_cmd(cmd_buffer);
    }
}
