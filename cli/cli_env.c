/* Copyright (C) 2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <cli.h>

typedef struct environment_variable_t environment_variable_t;

struct environment_variable_t
{
    char *name;
    char *value;
    environment_variable_t *next;
};

static environment_variable_t *environment_list_head = NULL;

const char *get_environment_variable(const char *name)
{
    environment_variable_t *e;

    for(e = environment_list_head; e; e = e->next)
        if(strcasecmp(e->name, name)==0)
            return e->value;

    return NULL;
}

int get_environment_variable_int(const char *name, int default_value)
{
    const char *value;

    value = get_environment_variable(name);

    if(value == NULL)
        return default_value;

    return strtol(value, NULL, 0);
}

void set_environment_variable(const char *name, const char *val) // val=NULL will delete an entry
{
    // remove variable (we will set it again in next step if val != NULL)
    environment_variable_t *e;
    environment_variable_t **ptr;
    ptr = &environment_list_head;
    e = environment_list_head;

    while(e){
        if(strcasecmp(e->name, name)==0){
            *ptr = e->next;
            free(e->name);
            free(e->value);
            free(e);
            break;
        }
        ptr = &e->next;
        e = e->next;
    }

    if(val){
        // set variable
        e = malloc(sizeof(environment_variable_t));
        e->name = strdup(name);
        e->value = strdup(val);
        // convert e->name to lower case
        for(char *p=e->name; *p; p++) 
            *p = tolower(*p);
        // add to linked list
        e->next = environment_list_head;
        environment_list_head = e;
    }
}

void do_set(char *argv[], int argc)
{
    environment_variable_t *e;

    if(argc == 0){
        // no args -- print environment
        int count = 0;
        for(e = environment_list_head; e; e = e->next){
            printf("%s=%s\n", e->name, e->value);
            count++;
        }
        printf("(%d environment variables)\n", count);
        return;
    }else{
        const char *name, *val = NULL;
        name = argv[0]; /* we know already argc > 0 */
        if(strchr(name, '='))
            printf("set: please use \"set a b\", not \"set a=b\"\n");
        else{
            if(argc == 2)
                val = argv[1];
            set_environment_variable(name, val);
        }
    }
}

int get_base(void)
{
    return get_environment_variable_int("base", 0);
}

uint32_t parse_uint32(const char *cptr, const char **endptr)
{
    int base = get_base();

    /* regardless of configured base value we always recognise 0x for hex */
    if(cptr[0]=='0' && (cptr[1] & 0xDF) == 'X'){
        base = 0; /* strtoul will switch to hex */
    }

    return strtoul(cptr, endptr, base);
}
