/* this is based in part on main68.c from KISS-BIOS 
 * Portions Copyright (C) 2011-2016 John R. Coffman.
 * Portions Copyright (C) 2015-2023 William R. Sowerbutts
 */

#include <types.h>
#include <stdlib.h>
#include <fatfs/ff.h>
#include <bootinfo.h>
#include <cli.h>
#include <cpu.h>
#include <net.h>
#include <uart.h>
#include <loader.h>

#define AUTOBOOT_FILENAME "boot"
#define AUTOBOOT_TIMEOUT_MS 500 /* this is actually enough as you can pre-stuff the UART receiver */

#define MAXARG 40
#define LINELEN 2048
char *cmd_buffer;

static void execute_cmd(char *linebuffer);
static void handle_any_command(char *argv[], int argc);

const cmd_entry_t builtin_cmd_table[] = {
    /* -- cli_fs.c --------------------- */
    /* name         min     max function */
    {"cd",          1,      1,  &do_cd,       "change directory <dir>"},
    {"del",         1, MAXARG,  &do_rm,       "delete a file" },
    {"dir",         0,      1,  &do_ls,       "list directory [<vol>:]"  },
    {"ls",          0,      1,  &do_ls,       "synonym for DIR"  },
    {"mkdir",       1,      1,  &do_mkdir,    "make a directory" },
    {"mv",          2,      2,  &do_mv,       "rename a file" },
    {"cp",          2,      2,  &do_cp,       "copy a file" },
    {"copy",        2,      2,  &do_cp,       "copy a file" },
    {"rename",      2,      2,  &do_mv,       "rename a file" },
    {"rm",          1, MAXARG,  &do_rm,       "delete a file" },

    /* -- cli_env.c -------------------- */
    /* name         min     max function */
    {"set",         0,      2,  &do_set,      "show or set environment variables" },

    /* -- cli_mem.c -------------------- */
    /* name         min     max function */
    {"dm",          2,      2,  &do_dump,     "synonym for DUMP" },
    {"dump",        2,      2,  &do_dump,     "dump memory <from> <count>" },
    {"wm",          2,      0,  &do_writemem, "synonym for WRITEMEM"},
    {"writemem",    2,      0,  &do_writemem, "write memory <addr> [byte ...]" },

    /* -- cli_info.c ------------------- */
    /* name         min     max function */
    {"meminfo",    0,      0,   &do_meminfo,  "info on memory state" },
    {"netinfo",     0,      0,  &do_netinfo,  "network statistics" },
    {"help",        0,      0,  &help,        "list this help info"   },

    /* -- cli_tftp.c ------------------- */
    /* name         min     max function */
    {"tftp",        1,      3,  &do_tftp,     "retrieve file with TFTP" },

    /* -- terminator ------------------- */
    {0, 0, 0, 0, 0 }

    /* targets also define a target_cmd_table[] of additional commands */
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

    if(handle_cmd_drive(argv, argc))
        return;

    if(!handle_cmd_table(argv, argc, target_cmd_table) && 
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
    cmd_buffer = malloc(LINELEN);

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

    free(cmd_buffer);
    cmd_buffer = 0;
}
