/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>

/*
 * A minimal implementation of selected standard C library functions
 *
 * Borrowed from the Fuzix source code
 *
 * Some code taken from the Linux-8086 C library, distributed under the GNU
 * Library General Public License.  
 * Copyright (C) 1995,1996 Robert de Bath <rdebath@cix.compulink.co.uk>
 *
 * Some code ANSIfied from dLibs 1.2
 */

int errno;

char *strcpy(char *d, const char *s)
{
        return memcpy(d, s, strlen(s) + 1);
}

char *strcat(char *d, const char *s)
{
	strcpy(d + strlen(d), s);
	return d;
}

int strcasecmp(const char *s, const char *d)
{
    int a, b;
   for(;;)
   {
      if( *s != *d )
      {
          a = tolower(*s);
          b = tolower(*d);
	 if( a != b )
	    return a - b;
      }
      else if( *s == '\0' ) break;
      s++; d++;
   }
   return 0;
}

int strncasecmp(const char *s, const char *d, size_t l)
{
    int a, b;
   while(l>0)
   {
      if( *s != *d )
      {
          a = tolower(*s);
          b = tolower(*d);
	 if( a != b )
	    return a - b;
      }
      else
	 if( *s == '\0' ) return 0;
      s++; d++; l--;
   }
   return 0;
}

char *strchr(const char *s, int c)
{
	register char ch;

	for (;;) {
		if ((ch = *s) == c)
			return (char*)s;
		if (ch == 0)
			return 0;
		s++;
	}
}

void *memchr(const void *str, int c, size_t l)
{
	register const char *p = str;

	while (l-- != 0) {
		if (*p == c)
			return (void*) p;
		p++;
	}
	return NULL;
}

char *strncat(char *d, const char *s, size_t l)
{
        register char *s1 = d + strlen(d), *s2 = memchr(s, 0, l);

        if (s2)
                memcpy(s1, s, s2 - s + 1);
        else {
                memcpy(s1, s, l);
                s1[l] = '\0';
        }
        return d;
}

char *strncpy(char *d, const char *s, size_t l)
{
        register char *s1 = d;
        register const char *s2 = s;

        while (l) {
                l--;
                if ((*s1++ = *s2++) == '\0')
                        break;
        }

        /* This _is_ correct strncpy is supposed to zap */
        while (l-- != 0)
                *s1++ = '\0';
        return d;
}

int memcmp(const void *mem1, const void *mem2, size_t len)
{
        const signed char *p1 = mem1, *p2 = mem2;

        if (!len)
                return 0;

        while (--len && *p1 == *p2) {
                p1++;
                p2++;
        }
        return *p1 - *p2;
}

int strncmp(const char *str1, const char *str2, size_t limit)
{
        for(; ((--limit) && (*str1 == *str2)); ++str1, ++str2)
                if (*str1 == '\0')
                        return 0;
        return *str1 - *str2;
}

int strcmp(const char *d, const char *s)
{
        register char *s1 = (char *) d, *s2 = (char *) s, c1, c2;

        while ((c1 = *s1++) == (c2 = *s2++) && c1);
        return c1 - c2;
}

int isalnum(int c)
{
        return isdigit(c) || isalpha(c);
}

int isalpha(int c)
{
        return isupper(c) || islower(c);
}

int isascii(int c)
{
        return !((uint8_t)c & 0x80);
}

int isblank(int c)
{
        return ((uint8_t)c == ' ') || ((uint8_t)c == '\t');
}

int iscntrl(int c)
{
        return (((uint8_t)c >= 0) && ((uint8_t)c <= 31)) || ((uint8_t)c == 127);
}

int isdigit(int c)
{
        return ((uint8_t)c >= '0') && ((uint8_t)c <= '9');
}

int isgraph(int c)
{
        return ((uint8_t)c >= 33) && ((uint8_t)c <= 126);
}

int islower(int c)
{
        return ((uint8_t)c >= 'a') && ((uint8_t)c <= 'z');
}

int isprint(int c)
{
        return ((uint8_t)c >= 32) && ((uint8_t)c <= 126);
}

int ispunct(int c)
{
        return isascii(c) && !iscntrl(c) && !isalnum(c) && !isspace(c);
}

int isspace(int c)
{
        return ((uint8_t)c == ' ')  ||
               ((uint8_t)c == '\t') ||
               ((uint8_t)c == '\n') ||
               ((uint8_t)c == '\r') ||
               ((uint8_t)c == '\f') ||
               ((uint8_t)c == '\v');
}

int isupper(int c)
{
        return ((uint8_t)c >= 'A') && ((uint8_t)c <= 'Z');
}

int isxdigit(int c)
{
        uint8_t bc = c;
        if (isdigit(bc))
                return 1;
        bc |= 0x20;
        return ((bc >= 'a') && (bc <= 'f'));
}

int tolower(int c)
{
	uint8_t cb = c;
	if ((cb >= 'A') && (cb <= 'Z'))
		cb ^= 0x20;
	return cb;
}

int toupper(int c)
{
	uint8_t cb = c;
	if ((cb >= 'a') && (cb <= 'z'))
		cb ^= 0x20;
	return cb;
}

/* we need to disable some of gcc's optimisations for these 
 * functions to prevent it converting them into a recursive call
 * back to themselves! */
#ifdef __GNUC__
__attribute__ ((__optimize__ ("-fno-tree-loop-distribute-patterns"))) 
#endif
size_t strlen(const char *t)
{
        size_t ct = 0;
        while (*t++)
                ct++;
        return ct;
}

#if 0 /* simple memcpy/memmove/memset */
#ifdef __GNUC__
__attribute__ ((__optimize__ ("-fno-tree-loop-distribute-patterns"))) 
#endif
void *memcpy(void *dest, const void *src, size_t len)
{
        uint8_t *dp = dest;
        const uint8_t *sp = src;
        while(len-- > 0)
                *dp++=*sp++;
        return dest;
}

#ifdef __GNUC__
__attribute__ ((__optimize__ ("-fno-tree-loop-distribute-patterns"))) 
#endif
void *memset(void *dest, int data, size_t len)
{
        char *p = dest;
        char v = (char)data;

        while(len--)
                *p++ = v;

        return dest;
}

#ifdef __GNUC__
__attribute__ ((__optimize__ ("-fno-tree-loop-distribute-patterns"))) 
#endif
void *memmove(void *dest, const void *src, size_t len)
{
        uint8_t *dp = dest;
        const uint8_t *sp = src;

        if (sp < dp) {
                dp += len;
                sp += len;
                while(len--)
                        *--dp = *--sp;
        } else {
                while(len--)
                        *dp++ = *sp++;
        }
        return dest;
}
#endif
