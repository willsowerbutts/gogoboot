#ifndef __CLI_DOT_H__
#define __CLI_DOT_H__

void command_line_interpreter(void);
const char *f_errmsg(int errno);
void f_perror(int errno);
const char *get_environment_variable(const char *name);

typedef struct
{
    const char *name;
    const int min_args;
    const int max_args;
    void (* function)(char *argv[], int argc);
    const char *helpme;
} cmd_entry_t;

extern const cmd_entry_t target_cmd_table[];

#endif
