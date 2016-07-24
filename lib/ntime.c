/*
 * ntime.c
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
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE < 199309L)
    #undef _POSIX_C_SOURCE
#endif
#define _POSIX_C_SOURCE 199309L

#include <ntime.h>

#if defined(__unix__) || defined(__unix) || defined(unix) || \
    (defined(__APPLE__) && defined(__MACH__))
    #include <unistd.h>
#endif

#if !defined(_POSIX_TIMERS) || !(_POSIX_TIMERS > 0) || \
    !defined(CLOCK_REALTIME)
    #error "POSIX timers required but not available!"
#endif

/*
 Select the monotonic clock source best suited for relative timing
 measurements.

 Note 1:
 CLOCK_MONOTONIC_RAW provided by modern Linux based systems seems a
 better choice over CLOCK_MONOTONIC, since it is guaranteed to not
 be influenced by NTP slew, thus running at a constant speed. The
 drawback is it might run slightly slower or faster than the adjusted
 monotonic clock.

 Note 2:
 Untested on AIX, BSD, Solaris.
*/
#if defined(CLOCK_MONOTONIC_PRECISE)
    /* BSD */
    #define NT_MONOCLOCK    CLOCK_MONOTONIC_PRECISE
#elif defined(CLOCK_MONOTONIC_RAW)
    /* Linux */
    #define NT_MONOCLOCK    CLOCK_MONOTONIC_RAW
#elif defined(CLOCK_HIGHRES)
    /* Solaris */
    #define NT_MONOCLOCK    CLOCK_HIGHRES
#elif defined(CLOCK_MONOTONIC)
    /* AIX, BSD, Linux, Solaris, and generally POSIX compliant */
    #define NT_MONOCLOCK    CLOCK_MONOTONIC
#else
    #error "No monotonic clock source available!"
#endif

#define NT_REALCLOCK    CLOCK_REALTIME


struct timeval *ntime_to_timeval( ntime_t t, struct timeval *tv )
{
    tv->tv_sec = t / NT_NS_PER_S;
    tv->tv_usec = t % NT_NS_PER_S / NT_NS_PER_US;
    return tv;
}

ntime_t ntime_from_timeval( struct timeval *tv )
{
    return (ntime_t)tv->tv_sec * NT_NS_PER_S + tv->tv_usec * NT_NS_PER_US;
}


struct timespec *ntime_to_timespec( ntime_t t, struct timespec *ts )
{
    ts->tv_sec = t / NT_NS_PER_S;
    ts->tv_nsec = t % NT_NS_PER_S;
    return ts;
}

ntime_t ntime_from_timespec( struct timespec *ts )
{
    return (ntime_t)ts->tv_sec * NT_NS_PER_S + ts->tv_nsec;
}


ntime_t ntime_get( void )
{
    struct timespec ts;
    if ( 0 != clock_gettime( NT_REALCLOCK, &ts ) )
        return -1;
    return ntime_from_timespec( &ts );
}

ntime_t ntime_res( void )
{
    struct timespec ts;
    if ( 0 != clock_getres( NT_REALCLOCK, &ts ) )
        return -1;
    return ntime_from_timespec( &ts );
}


ntime_t nclock_get( void )
{
    struct timespec ts;
    if ( 0 != clock_gettime( NT_MONOCLOCK, &ts ) )
        return -1;
    return ntime_from_timespec( &ts );
}

ntime_t nclock_res( void )
{
    struct timespec ts;
    if ( 0 != clock_getres( NT_MONOCLOCK, &ts ) )
        return -1;
    return ntime_from_timespec( &ts );
}

/* EOF */
