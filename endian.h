#ifndef AMF_ENDIAN_H
#   define AMF_ENDIAN_H

#ifdef __BYTE_ORDER
#   if __BYTE_ORDER == __LITTLE_ENDIAN
#	ifndef __LITTLE_ENDIAN__
#	    define __LITTLE_ENDIAN__
#	endif
#   elif __BYTE_ORDER == __BIG_ENDIAN
#	ifndef __BIG_ENDIAN__
#	    define __BIG_ENDIAN__
#	endif
#   endif
#endif

#ifdef __LITTLE_ENDIAN__
#   define NTOH64(x) __builtin_bswap64(x)
#   define NTOH32(x) __builtin_bswap32(x)
#   define NTOH16(x) (__builtin_bswap32((x) << 8) >> 8)
#elif defined __BIG_ENDIAN__
#   define NTOH64(x) (x)
#   define NTOH32(x) (x)
#   define NTOH16(x) (x)
#else
#   error "Cannot determine endian"
#endif

#endif
