#ifndef _HOSTED_H_
#define _HOSTED_H_

#ifdef HOSTED_PREFIX
#define __HOSTED_CONCAT(prefix, name) prefix##_##name
#define _HOSTED_CONCAT(prefix, name) __HOSTED_CONCAT(prefix, name)
#define HOSTED_API(name) _HOSTED_CONCAT(HOSTED_PREFIX, name)
#else
#define HOSTED_API(name) name
#endif

#endif /* _HOSTED_H_ */