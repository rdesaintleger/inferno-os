#ifndef _INFERNO_LOCK_TYPES_H_
#define _INFERNO_LOCK_TYPES_H_

/*
 *  Complete definitions of the types of <inferno/lock.h>, for the backend that the
 *  build selects.  Include it where a lock is embedded in a structure or defined;
 *  everywhere else the pointers declared by lock.h are enough.
 */
#include <inferno/lock.h>

#if defined(INFERNO_LOCK_BACKEND_PTHREAD)
#include <inferno/lock-types-pthread.h>
#elif defined(INFERNO_LOCK_BACKEND_COOP)
#include <inferno/lock-types-coop.h>
#else
#error "no lock backend: define INFERNO_LOCK_BACKEND_PTHREAD or INFERNO_LOCK_BACKEND_COOP"
#endif

#endif /* _INFERNO_LOCK_TYPES_H_ */
