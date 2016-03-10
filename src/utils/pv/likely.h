/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef LIKELY_H_
#define LIKELY_H_

#if defined(__GNUC__) && __GNUC__ >= 3
#define likely(x) __builtin_expect (x, 1)
#define unlikely(x) __builtin_expect (x, 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#endif /* LIKELY_H_ */
