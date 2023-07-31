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

/* network byte ordering functions */
#if __BYTE_ORDER == __BIG_ENDIAN
#define ntohl(x)        ((uint32_t)(x))
#define ntohs(x)        ((uint16_t)(x))
#define htonl(x)        ((uint32_t)(x))
#define htons(x)        ((uint16_t)(x))
#define cpu_to_le16(x)  ((uint16_t)le16_to_cpu(x))
#define le16_to_cpu(x)  ((uint16_t)(__builtin_bswap16((uint16_t)(x))))
#define cpu_to_le32(x)  ((uint32_t)le32_to_cpu(x))
#define le32_to_cpu(x)  ((uint32_t)(__builtin_bswap32((uint32_t)(x))))
#define cpu_to_be16(x)  ((uint16_t)(x))
#define be16_to_cpu(x)  ((uint16_t)(x))
#define cpu_to_be32(x)  ((uint32_t)(x))
#define be32_to_cpu(x)  ((uint32_t)(x))
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ntohl(x)        (__builtin_bswap_32((uint32_t)(x)))
#define ntohs(x)        (__builtin_bswap_16((uint16_t)(x)))
#define htonl(x)        (__builtin_bswap_32((uint32_t)(x)))
#define htons(x)        (__builtin_bswap_16((uint16_t)(x)))
#define cpu_to_le16(x)  ((uint16_t)(x))
#define le16_to_cpu(x)  ((uint16_t)(x))
#define cpu_to_le32(x)  ((uint32_t)(x))
#define le32_to_cpu(x)  ((uint32_t)(x))
#define cpu_to_be16(x)  ((uint16_t)be16_to_cpu(x))
#define be16_to_cpu(x)  ((uint16_t)(__builtin_bswap16((uint16_t)(x))))
#define cpu_to_be32(x)  ((uint32_t)be32_to_cpu(x))
#define be32_to_cpu(x)  ((uint32_t)(__builtin_bswap32((uint32_t)(x))))
#endif
#endif

#endif
