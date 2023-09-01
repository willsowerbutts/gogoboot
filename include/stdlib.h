#ifndef __STDLIB_DOT_H__SENTRY__
#define __STDLIB_DOT_H__SENTRY__

#include <types.h>

/* cli.c */
void pretty_dump_memory(void *start, int len);

/* -- printf.c -- */

int printf(char const *fmt, ...);
int puts(const char *s);
int putch(char ch);
int putchar(int ch);

/* target specific */
void halt(void);

/* malloc and friends */
void *malloc(size_t size); /* halts machine on out of memory */
void *malloc_unchecked(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size); /* halts machine on out of memory */

/* -- stdlib.c -- */

extern int errno;

char *strdup(const char *s);
char *strcat(char *d, const char *s);
int strcasecmp(const char *s, const char *d);
int strncasecmp(const char *s, const char *d, size_t l);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strncat(char *d, const char *s, size_t l);
size_t strlen(const char *t);
char *strchr(const char *s, int c);
int strncmp(const char *str1, const char *str2, size_t limit);
int strcmp(const char *d, const char *s);
void *memcpy(void *dest, const void *src, size_t len);
int memcmp(const void *mem1, const void *mem2, size_t len);
void *memchr(const void *str, int c, size_t l);
void *memmove(void *dest, const void *src, size_t len);
void *memset(void *dest, int data, size_t len);
int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

/* -- strtoul.c -- */

int atoi(const char *cp);
unsigned long strtoul(const char *cptr, const char **endptr, int radix);
long strtol(const char *nptr, const char **endptr, int base);

/* -- qsort.c -- */
void qsort(void *base, size_t nel, size_t width, int (*cmp)(const void *, const void *));

/* limits */

/* Number of bits in a `char'.	*/
#  define CHAR_BIT	8

/* Minimum and maximum values a `signed char' can hold.  */
#  define SCHAR_MIN	(-128)
#  define SCHAR_MAX	127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0.)  */
#  define UCHAR_MAX	255

/* Minimum and maximum values a `char' can hold.  */
#  ifdef __CHAR_UNSIGNED__
#   define CHAR_MIN	0
#   define CHAR_MAX	UCHAR_MAX
#  else
#   define CHAR_MIN	SCHAR_MIN
#   define CHAR_MAX	SCHAR_MAX
#  endif

/* Minimum and maximum values a `signed short int' can hold.  */
#  define SHRT_MIN	(-32768)
#  define SHRT_MAX	32767

/* Maximum value an `unsigned short int' can hold.  (Minimum is 0.)  */
#  define USHRT_MAX	65535

/* Minimum and maximum values a `signed int' can hold.  */
#  define INT_MIN	(-INT_MAX - 1)
#  define INT_MAX	2147483647

/* Maximum value an `unsigned int' can hold.  (Minimum is 0.)  */
#  define UINT_MAX	4294967295U

/* Minimum and maximum values a `signed long int' can hold.  */
#  if __WORDSIZE == 64
#   define LONG_MAX	9223372036854775807L
#  else
#   define LONG_MAX	2147483647L
#  endif
#  define LONG_MIN	(-LONG_MAX - 1L)

/* Maximum value an `unsigned long int' can hold.  (Minimum is 0.)  */
#  if __WORDSIZE == 64
#   define ULONG_MAX	18446744073709551615UL
#  else
#   define ULONG_MAX	4294967295UL
#  endif

#  ifdef __USE_ISOC99

/* Minimum and maximum values a `signed long long int' can hold.  */
#   define LLONG_MAX	9223372036854775807LL
#   define LLONG_MIN	(-LLONG_MAX - 1LL)

/* Maximum value an `unsigned long long int' can hold.  (Minimum is 0.)  */
#   define ULLONG_MAX	18446744073709551615ULL

#  endif /* ISO C99 */


#endif
