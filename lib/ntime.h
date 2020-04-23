/*
 * ntime.h
 *
 * Get and convert absolute and relative times expressed in nanoseconds
 * represented as simple integral objects. Accuracy depends on the
 * underlying implementation and hardware.
 *
 * The getter functions are basically convenience wrappers for the POSIX
 * clock_gettime() and clock_getres() interface, which must be available
 * on the target system. (Link with librt, if required.)
 *
 * Copyright 2013-2016 Urban Wallasch <irrwahn35@freenet.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */


#ifndef NTIME_H_INCLUDED
#define NTIME_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <time.h>
#include <sys/time.h>


/*
 Conversion specifier string macros suitable for use in fprintf
 and fscanf format strings.
*/
#define PRI_ntime       PRIdLEAST64
#define SCA_ntime       SCAdLEAST64

/*
 Guess what. Got tired of typing zeroes.
*/
#define NT_NS_PER_S     (1000*1000*1000)
#define NT_NS_PER_MS    (1000*1000)
#define NT_NS_PER_US    (1000)

#define NT_US_PER_S     (1000*1000)
#define NT_US_PER_MS    (1000)

#define NT_MS_PER_S     (1000)

/*
 Type to hold ntime values. Capable of holding absolute time
 values in nanosecond resolution at least up until the year 2262.
*/
typedef int_least64_t ntime_t;


/*
 Convert between ntime_t and other common time representations.
 No rounding is applied so that results should agree with those
 returned by time() and gettimeofday(). Conversions to and from
 structural types are implemented as functions to steer clear
 of macros evaluating their arguments more than once.

 The (in principle good) idea to use inline functions here was
 dropped due to the incomprehensible mess of 'inline' semantics
 across various C standards and compilers. Instead, use a high
 optimization setting and let the compiler figure out the best
 way to deal with it. Sigh. [uw 2014-03-21]
*/

#define ntime_to_us(ns)       ( (ns) / NT_NS_PER_US )
#define ntime_from_us(us)     ( (ntime_t)(us) * NT_NS_PER_US )

#define ntime_to_ms(ns)       ( (ns) / NT_NS_PER_MS )
#define ntime_from_ms(ms)     ( (ntime_t)(ms) * NT_NS_PER_MS )

#define ntime_to_s(ns)        ( (ns) / NT_NS_PER_S )
#define ntime_from_s(s)       ( (ntime_t)(s) * NT_NS_PER_S )

#define ntime_to_time_t(ns)   ( (time_t)ntime_to_s((ns)) )
#define ntime_from_time_t(s)  ( ntime_from_s((s)) )

extern struct timeval *ntime_to_timeval( ntime_t t, struct timeval *tv );
extern ntime_t ntime_from_timeval( struct timeval *tv );

extern struct timespec *ntime_to_timespec( ntime_t t, struct timespec *ts );
extern ntime_t ntime_from_timespec( struct timespec *ts );


/*
 Get real ("wall-clock") time in nanoseconds since UNIX epoch.
 This may experience discontinuous jumps due to time adjustments.
 Use this in (absolute) calendar time calculations.
 Returns -1 on error.
*/
extern ntime_t ntime_get( void );

/*
 Get real ("wall-")clock resolution in nanoseconds.
 Returns -1 on error.
*/
extern ntime_t ntime_res( void );

/*
 Get monotonic clock time in nanoseconds since arbitrary epoch,
 usually boot time on Linux systems; guaranteed to not "jump" due
 to time adjustments, but _may_ be subject to NTP slew, depending
 on the target system. Best suited for (relative) timing purposes.
 Returns -1 on error.
*/
extern ntime_t nclock_get( void );

/*
 Get monotonic clock resolution in nanoseconds.
 Returns -1 on error.
*/
extern ntime_t nclock_res( void );


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif  /* ndef NTIME_H_INCLUDED */

/* EOF */
