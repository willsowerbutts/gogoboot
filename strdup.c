#include <stdlib.h>

char *strdup(const char *s)
{
    int len = strlen(s) + 1;
    char *p = malloc(len);
    memcpy(p, s, len);
    return p;
}
