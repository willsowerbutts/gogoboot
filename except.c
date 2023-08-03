#include <stdlib.h>

const char * const vector_name[] = {
    NULL, // shouldn't happen
    NULL, // shouldn't happen
    "Access fault",
    "Address error",
    "Illegal instruction",
    "Divide by zero",
    "CHK/CHK2",
    "FTRAPcc/TRAPcc/TRAPV",
    "Privilege violation",
    "Trace",
    "Unimplemented opcode A",
    "Unimplemented opcode F",
    NULL, // reserved
    NULL, // undefined for 68040
    "Format error",
    "Uninitialised interrupt",
};

/* this is called by the assembler code in vectors.s */
void report_exception(uint16_t *sp)
{
    int vector = (*(sp+3) & 0xFFF) >> 2;
    const char *vecname = NULL;

    if(vector < sizeof(vector_name)/sizeof(char*))
        vecname = vector_name[vector];
    if(!vecname)
        vecname = "??? RTFM!";

    printf("\nCPU exception!\n");
    printf("sp=%08x sr=%04x pc=%04x%04x frame type %d vector %d (%s)\nStack contents:\n",
            (int)sp, *sp, *(sp+1), *(sp+2), *(sp+3) >> 12, vector, vecname);
    pretty_dump_memory(sp, 256);
}
