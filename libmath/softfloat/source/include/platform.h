/*
 * Platform configuration of SoftFloat for Inferno.
 *
 * This file replaces the build/<target>/platform.h of the upstream
 * distribution.  The upstream configuration macros have been resolved once
 * and for all in the sources (see ../../README.inferno): 64-bit integer
 * support is required, there is no endianness or compiler specific code,
 * and results do not depend on the host machine.
 *
 * Only one thing is left to the platform: how functions defined in headers
 * are declared.  `static inline' is C99; older compilers get `static'.
 */
#ifndef inferno_softfloat_platform_h
#define inferno_softfloat_platform_h

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define INLINE static inline
#elif defined(__GNUC__)
#define INLINE static __inline__
#else
#define INLINE static
#endif

/*
 * THREAD_LOCAL is left undefined (empty): SoftFloat's state (rounding mode,
 * exception flags, tininess mode, extF80 rounding precision) is global.
 * Inferno saves and restores it per process (see libmath/fp).
 */

#endif
