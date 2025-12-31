/******************************************************************************
 * bits256 utility functions and macros
 * These are simple utility functions for bits256 operations
 ******************************************************************************/

#ifndef BITS256_UTILS_H
#define BITS256_UTILS_H

#include "curve25519.h"
#include <pthread.h>

// Bit manipulation macros (from OS_portable.h)
#define SETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] |= (1 << ((bitoffset) & 7)))
#define GETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] & (1 << ((bitoffset) & 7)))
#define CLEARBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] &= ~(1 << ((bitoffset) & 7)))

// bits256 utility macros
#define bits256_nonz(a) (((a).ulongs[0] | (a).ulongs[1] | (a).ulongs[2] | (a).ulongs[3]) != 0)

// Thread creation macro (from OS_portable.h)
#define OS_thread_create pthread_create

// Forward declarations for functions defined in libs/crypto/iguana_utils.c
// These are simple utility functions, not iguana-specific
char *bits256_str(char hexstr[65], bits256 x);
bits256 bits256_conv(char *hexstr);
int32_t bits256_cmp(bits256 a, bits256 b);

// Hex encoding/decoding functions (from OS_portable.h)
int32_t decode_hex(uint8_t *bytes, int32_t n, char *hex);
int32_t init_hexbytes_noT(char *hexbytes, uint8_t *message, long len);

// String utility functions (from OS_portable.h)
int32_t safecopy(char *dest, char *src, long len);

// OS_init function (from OS_portable.h)
void OS_init(void);

#endif // BITS256_UTILS_H

