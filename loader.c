/* Copyright (C) 2015-2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <fatfs/ff.h>
#include <elf.h>
#include <bootinfo.h>
#include <tinyalloc.h>
#include <net.h>
#include <cpu.h>
#include <cli.h>
#include <init.h>

extern const char bss_end;

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

void execute(void *entry_vector)
{
    printf("Entry at 0x%lx in supervisor mode\n", (uint32_t)entry_vector);
    cpu_interrupts_off();
    cpu_cache_flush();
    machine_execute(entry_vector); /* inside here we need to move any bounce buffer into place */
    /* we're back? */
    cpu_interrupts_on();
}

static FRESULT load_executable_data(FIL *fd, uint32_t paddr, uint32_t offset, uint32_t size)
{
    unsigned int bytes_read;
    FRESULT fr;
    
    printf("Loading %lu bytes from file offset 0x%lx to memory at 0x%lx\n", size, offset, paddr);

    /* check that this makes sense */
    if(paddr + size > ram_size){
        printf("Abort: load would go past end of RAM\n");
        return FR_DISK_ERR;
    }
    if(paddr + size > heap_base){
        printf("Abort: load would overlap heap memory\n");
        return FR_DISK_ERR;
    }
    if(paddr < (uint32_t)&bss_end){
        printf("Abort: load would overlap gogoboot memory\n");
        return FR_DISK_ERR;
        /* BUT ... WE CAN FIX IT -- with a bounce buffer! */
    }

    printf("-> Load %lu bytes from file offset 0x%lx to memory at 0x%lx\n", size, offset, paddr);

    fr = f_lseek(fd, offset);
    if(fr != FR_OK)
        return fr;

    fr = f_read(fd, (char*)paddr, size, &bytes_read);
    if(fr != FR_OK)
        return fr;
    if(bytes_read != size){
        printf("short read (wanted %ld got %d)\n", size, bytes_read);
        return FR_DISK_ERR;
    }
    return FR_OK;
}

bool load_m68k_executable(char *argv[], int argc, FIL *fd)
{
    // TODO choose a better load address
    // On Q40 SMSQ/E does not like being loaded at 256KB. 2048KB seems fine. Maybe it copies itself downwards?
    // 2MB is pretty high up!
    // maybe we should require the user to tell us the load address?
    uint32_t load_address = 2048*1024; 
    FRESULT fr;

    fr = load_executable_data(fd, load_address, 0, f_size(fd));
    if(fr != FR_OK){
        printf("%s: Cannot load: ", argv[0]);
        f_perror(fr);
        return false;
    }

    execute((void*)load_address);
    return true; /* unlikely we will return ... */
}

bool load_elf_executable(char *arg[], int numarg, FIL *fd)
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
#if 0
                /* WRS - is this necessary in gogoboot? */
                if(proghead.paddr == 0){
                    /* newer linkers include the header in the first segment. fixup. */
                    proghead.offset += 0x1000;
                    proghead.paddr += 0x1000;
                    proghead.filesz -= 0x1000;
                    proghead.memsz -= 0x1000;
                }
#endif
#if 0
                /* WRS - trying to live without a fixed offset */
                proghead.paddr += EXECUTABLE_LOAD_ADDRESS;
#endif
                if(load_executable_data(fd, proghead.paddr, proghead.offset, proghead.filesz) != FR_OK){
                    printf("Unable to load segment from ELF file.\n");
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
        execute((void*)(header.entry));
    }

    return true;
}
