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
