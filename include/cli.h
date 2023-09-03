#ifndef __CLI_DOT_H__
#define __CLI_DOT_H__

#include <fatfs/ff.h>

void command_line_interpreter(void);

// environment variables
const char *get_environment_variable(const char *name);
void set_environment_variable(const char *name, const char *val); // val=NULL will delete an entry

// fat_fs extensions
const char *f_errmsg(int errno);
void f_perror(int errno);

// execute loaded code (wrapper that ultimately calls machine_execute)
void execute(void *entry_vector);
FRESULT load_data(FIL *fd, uint32_t paddr, uint32_t offset, uint32_t file_size, uint32_t size);

typedef struct
{
    const char *name;
    const int min_args;
    const int max_args;
    void (* function)(char *argv[], int argc);
    const char *helpme;
} cmd_entry_t;

extern const cmd_entry_t target_cmd_table[];
extern const cmd_entry_t builtin_cmd_table[];

// cli_fs.c
void do_cd(char *argv[], int argc);
void do_ls(char *argv[], int argc);
void do_rm(char *argv[], int argc);
void do_mkdir(char *argv[], int argc);
void do_mv(char *argv[], int argc);
void do_cp(char *argv[], int argc);

// cli_env.c
void do_set(char *argv[], int argc);

// cli_mem.c
void do_dump(char *argv[], int argc);
void do_writemem(char *argv[], int argc);

// cli_info.c
void help(char *argv[], int argc);
void do_meminfo(char *argv[], int argc);
void do_netinfo(char *argv[], int argc);

// cli_tftp.c
void do_tftp(char *argv[], int argc);

// cli_load.c
void do_execute(char *argv[], int argc);
void do_load(char *argv[], int argc);

#endif
