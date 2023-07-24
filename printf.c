/* Copyright (C) 1996 Robert de Bath <robert@mayday.compulink.co.uk>
 * This file is part of the Linux-8086 C library and is distributed
 * under the GNU Library General Public License.
 */

/* Modified 14-Jan-2002 by John Coffman <johninsd@san.rr.com> for inclusion
 * in the set of LILO diagnostics.  This code is the property of Robert
 * de Bath, and is used with his permission.
 */

/* Modified 14-Sep-2010 by John Coffman <johninsd@gmail.com> for use with
 * the N8VEM SBC-188 BIOS project.
 */

/* Modified 18-Aug-2011 by John Coffman <johninsd@gmail.com> for use with
 * the N8VEM baby M68k CPU board project.
 */

/* 2023-07-24 William R Sowerbutts -- modified for Q40BOOT */

#include <stdlib.h>
#include <stdarg.h>
#include "q40uart.h"

#define NUMLTH 11
static unsigned char * __numout(long i, int base, unsigned char out[]);

int putch(char ch)
{
    if (ch == '\n') 
        uart_write_byte('\r');
    uart_write_byte(ch);
    return (int)ch;
}

int puts(const char *s)
{
    int r;
    r = uart_write_str(s);
    putch('\n');
    return r + 1;
}

int printf(const char * fmt, ...)
{
   register int c;
   int count = 0;
   int type, base;
   long val;
   char * cp;
   char padch=' ';
   int  minsize, maxsize;
   unsigned char out[NUMLTH+1];
   va_list ap;

   va_start(ap, fmt);

   while((c=*fmt++))
   {
      count++;
      if(c!='%')
      {
	 putch(c);
      }
      else
      {
	 type=1;
	 padch = *fmt;
	 maxsize=minsize=0;
	 if(padch == '-') fmt++;

	 for(;;)
	 {
	    c=*fmt++;
	    if( c<'0' || c>'9' ) break;
	    minsize*=10; minsize+=c-'0';
	 }

	 if( c == '.' )
	    for(;;)
	    {
	       c=*fmt++;
	       if( c<'0' || c>'9' ) break;
	       maxsize*=10; maxsize+=c-'0';
	    }

	 if( padch == '-' ) minsize = -minsize;
	 else
	 if( padch != '0' ) padch=' ';

	 if( c == 0 ) break;
	 if(c=='h')
	 {
	    c=*fmt++;
	    type = 0;
	 }
	 else if(c=='l')
	 {
	    c=*fmt++;
	    type = 2;
	 }

	 switch(c)
	 {
	    case 'X':
	    case 'x': base=16; type |= 4;   if(0) {
	    case 'o': base= 8; type |= 4; } if(0) {
	    case 'u': base=10; type |= 4; } if(0) {
	    case 'd': base=-10; }
	       switch(type)
	       {
		  case 0: val=(short)(va_arg(ap, int)); break;		/* short is promoted to int by ... */
		  case 1: val=va_arg(ap, int);   break;
		  case 2: val=va_arg(ap, long);  break;
		  case 4: val=(unsigned short)va_arg(ap, unsigned int); break;	/* ditto */
		  case 5: val=va_arg(ap, unsigned int);   break;
		  case 6: val=va_arg(ap, unsigned long);  break;
		  default:val=0; break;
	       }
	       cp = (char*) __numout(val,base,out);
	       if(0) {
	    case 's':
	          cp=va_arg(ap, char *);
	       }
	       count--;
	       c = strlen(cp);
	       if( !maxsize ) maxsize = c;
	       if( minsize > 0 )
	       {
		  minsize -= c;
		  while(minsize>0) { putch(padch); count++; minsize--; }
		  minsize=0;
	       }
	       if( minsize < 0 ) minsize= -minsize-c;
	       while(*cp && maxsize-->0 )
	       {
		  putch(*cp++);
		  count++;
	       }
	       while(minsize>0) { putch(' '); count++; minsize--; }
	       break;
	    case 'c':
	       putch(va_arg(ap, int));			/* char is promoted to int by ... */
	       break;
	    default:
	       putch(c);
	       break;
	 }
      }
   }
   va_end(ap);
   return count;
}

const char nstring[]="0123456789ABCDEF";

static unsigned char *__numout(long i, int base, unsigned char out[])
{
   int n;
   int flg = 0;
   unsigned long val;

   if (base<0)
   {
      base = -base;
      if (i<0)
      {
	 flg = 1;
	 i = -i;
      }
   }
   val = i;

   out[NUMLTH] = '\0';
   n = NUMLTH-1;
   do
   {
      out[n--] = nstring[val % base];
      val /= base;
   }
   while(val);
   if(flg) out[n--] = '-';
   return &out[n+1];
}
