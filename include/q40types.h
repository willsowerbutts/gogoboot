/* mytypes.h	*/
/*
	Copyright (C) 2011 John R. Coffman.
	Licensed for hobbyist use on the N8VEM baby M68k CPU board.
***********************************************************************

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    in the file COPYING in the distribution directory along with this
    program.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************/
#ifndef __Q40TYPES_DOT_H__
#define __Q40TYPES_DOT_H__

typedef signed char		int8_t;
typedef signed short int	int16_t;
typedef signed long int		int32_t;
typedef signed long long	int64_t;

typedef unsigned char		uint8_t;
typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned long int	uint32_t;
typedef unsigned long long	uint64_t;

typedef int                     intptr_t;
typedef unsigned int            uintptr_t;

typedef uint32_t                size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define ntohs(x)        (x)
#define ntohl(x)        (x)

#define cpu_to_le16(x)  ((uint16_t)le16_to_cpu(x))
#define le16_to_cpu(x)  ((uint16_t)(__builtin_bswap16((uint16_t)(x))))
#define cpu_to_le32(x)  ((uint32_t)le32_to_cpu(x))
#define le32_to_cpu(x)  ((uint32_t)(__builtin_bswap32((uint32_t)(x))))

#endif
