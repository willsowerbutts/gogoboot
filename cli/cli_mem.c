/* Copyright (C) 2015-2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <init.h>
#include <cli.h>

void pretty_dump_memory(void *start, int len)
{
    int i, rem;
    unsigned char *ptr=(unsigned char *)start;
    char linebuffer[17], *lbptr;

    for(i=0;i<16;i++)
        linebuffer[i] = ' ';
    linebuffer[16]=0;
    lbptr = &linebuffer[0];

    printf("0x%08x ", (unsigned)ptr&(~15));
    for(i=0; i<((unsigned)ptr & 15); i++){
        printf("   ");
        lbptr++;
    }
    if(((unsigned)ptr & 15) > 8)
        putch(' ');

    while(len){
        if(*ptr >= 32 && *ptr < 127)
            *lbptr = *ptr;
        else
            *lbptr = '.';

        if (((unsigned)ptr & 0x0F) == 0x08)
            putch(' ');
        printf(" %02x", *ptr++);
        len--;
        lbptr++;

        if((unsigned)ptr % 16 == 0){
            printf("  %s", linebuffer);
            lbptr = &linebuffer[0];
            if(len)
                printf("\n0x%08x ", (int)ptr);
            else{
                /* no ragged end to tidy up! */
                printf("\n");
                return;
            }
        }
    }

    rem = 2 + 3 * (16 - ((unsigned)ptr & 15));

    if (rem >= 26)
        rem++;

    for(i=0; i<rem; i++)
        putch(' ');

    *lbptr = 0;
    puts(linebuffer);
}

void do_dump(char *argv[], int argc)
{
    unsigned long start, count;

    start = parse_uint32(argv[0], NULL);
    count = parse_uint32(argv[1], NULL);

    pretty_dump_memory((void*)start, count);
}

void do_memtest(char *argv[], int argc)
{
    unsigned long start, count;

    switch(argc){
        case 0:
            start = bounce_below_addr;
            if(heap_base > ram_size)
                count = ram_size - start;
            else
                count = heap_base - start;
            break;
        case 2:
            start = parse_uint32(argv[0], NULL);
            count = parse_uint32(argv[1], NULL);
            break;
        default:
            printf("memtest: wrong number of arguments\n" \
                   "memtest -- test all free memory\n" \
                   "memtest [start] [count] -- test specified region only\n");
            return;
    }

    memory_test(start, count);
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

void do_writemem(char *argv[], int argc)
{
    unsigned long value;
    unsigned char *ptr;
    int i, j, l;

    value = parse_uint32(argv[0], NULL);
    ptr = (unsigned char*)value;

    /* This can deal with values like: 1, 12, 1234, 123456, 12345678.
       Values > 2 characters are interpreted as big-endian words ie
       "12345678" is the same as "12 34 56 78" */

    /* NOTE that data input is ALWAYS in hex */

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
            *(ptr++) = strtoul(argv[i], NULL, 16); /* ALWAYS in hex */
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

