#ifndef _HOSTED_H_
#define _HOSTED_H_

#ifdef HOSTED_PREFIX
#define __HOSTED_CONCAT(prefix, name) prefix##_##name
#define _HOSTED_CONCAT(prefix, name) __HOSTED_CONCAT(prefix, name)
#define HOSTED_API(name) _HOSTED_CONCAT(HOSTED_PREFIX, name)
#else
#define HOSTED_API(name) name
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define	nil NULL

typedef uint8_t uchar;
typedef int8_t schar;
typedef uint32_t Rune;
typedef int64_t vlong;
typedef uint64_t uvlong;
typedef uint32_t u32int;
typedef uvlong u64int;

typedef uint32_t mpdigit;
typedef uint16_t u16int;
typedef uint8_t u8int;
typedef uintptr_t uintptr;

#endif /* _HOSTED_H_ */