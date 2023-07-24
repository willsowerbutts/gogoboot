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
#ifndef _MYTYPES_H
#define _MYTYPES_H 1

typedef signed char		int8_t;
typedef signed short int	int16_t;
typedef signed long int		int32_t;
typedef signed long long	int64_t;

typedef unsigned char		uint8_t;
typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned long int	uint32_t;
typedef unsigned long long	uint64_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif	/* _MYTYPES_H */

