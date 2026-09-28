#ifndef _INFERNO_PLATFORM_H_
#define _INFERNO_PLATFORM_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void *inferno_malloc(size_t size, size_t *got);
void inferno_free(void *ptr, size_t size);
void inferno_panic(char *fmt, ...);

#endif /* _INFERNO_PLATFORM_H_ */
