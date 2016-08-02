/*
 * logprintf.c
 *
 * Simple, configurable logging to file pointer and/or system log.
 * For LOG_TO_SYSLOG to work it needs to be enabled at compile time.
 * Requires linking with libpthread, if pthread support was enabled
 * at compile time.
 *
 * Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
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


#define _POSIX_C_SOURCE 200809L     /* localtime_r, strerror_r */

#if defined(WITH_SYSLOG) && !defined(WITH_OWN_VSYSLOG)
    #define _DEFAULT_SOURCE         /* vsyslog */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>
#include <sys/time.h>

#ifdef WITH_SYSLOG
    #include <syslog.h>
#endif

#ifdef WITH_PTHREAD
    #include <pthread.h>
    static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    #define LOCK_LOG()      pthread_mutex_lock( &mtx )
    #define UNLOCK_LOG()    pthread_mutex_unlock( &mtx )
#else
    #define LOCK_LOG()
    #define UNLOCK_LOG()
#endif /* WITH_PTHREAD */

#include <ntime.h>
#include <logprintf.h>


#define NULLSTR "(null)"

static struct {
    int init;
    int level;
    unsigned mode;
    const char *ident;
    FILE *file;
} cfg = {
    0,
    0,
    0,
    NULL,
    NULL
};


/*
 * Module static functions.
 */

#ifdef WITH_SYSLOG
#ifdef WITH_OWN_VSYSLOG
static void vsyslog_( int pri, const char *fmt, va_list arglist )
{
    va_list argcopy;
    va_copy( argcopy, arglist );
    int n = vsnprintf( NULL, 0, fmt, argcopy );
    va_end( argcopy );
    char buf[n];
    vsnprintf( buf, sizeof buf, fmt, arglist );
    syslog( pri, "%s", buf );
    return;
}
#else
#define vsyslog_(...) vsyslog(__VA_ARGS__)
#endif
#endif /* WITH_SYSLOG */

static void logprintf_init_( int lvl, const char *id, unsigned mode, FILE *fp )
{
    cfg.level = lvl;
    cfg.ident = id ? id : NULLSTR;
    cfg.mode  = mode;
    cfg.file  = fp ? fp : stderr;
#ifdef WITH_SYSLOG
    if ( cfg.mode & LOG_TO_SYSLOG )
    {
        closelog();
        openlog( cfg.ident, LOG_PID, LOG_USER );
    }
#endif
    cfg.init = 1;
    return;
}


/*
 * Exported functions.
 */

void logprintf_init( int lvl, const char *id, unsigned mode, FILE *fp )
{
    LOCK_LOG();
    logprintf_init_( lvl, id, mode, fp );
    UNLOCK_LOG();
}


void vlogprintf( int pri, const char *fmt, va_list arglist )
{
    int eno = errno;
    struct timeval tv;  /* used in file mode only */

    ntime_to_timeval( ntime_get(), &tv );
    LOCK_LOG();
    if ( !cfg.init )
        logprintf_init_( LOG_DEBUG, NULL, LOG_TO_FILE, stderr );
    if ( pri <= cfg.level )
    {
#ifdef WITH_SYSLOG
        if ( cfg.mode & LOG_TO_SYSLOG )
        {
            va_list argcopy;
            va_copy( argcopy, arglist );
            errno = eno;
            vsyslog_( pri, fmt, argcopy );
            va_end( argcopy );
        }
#endif
        if ( cfg.mode & LOG_TO_FILE )
        {
            /* write timestamp, ident, pid and priority */
            char tbuf[32];
            struct tm ts;
            localtime_r( &tv.tv_sec, &ts );
            strftime( tbuf, sizeof tbuf, "%FT%T", &ts );
            fprintf( cfg.file, "%s.%06d", tbuf, (int)tv.tv_usec );
            strftime( tbuf, sizeof tbuf, "%z", &ts );
            fprintf( cfg.file, "%.3s:%s %s[%u]:%d: ", tbuf, tbuf + 3,
                     cfg.ident, (unsigned)getpid(), pri );
            /* write the formatted message while successively
               replacing all %m sequences in format string */
            const char *f = fmt;
            const char *m = strstr( f, "%m" );
            if ( m )
            {
                /* The POSIX strerror_r() specification is insane, as
                   it does not include any mechanism to determine the
                   (maximum) length of the resulting string, except
                   trying until it stops failing. Thus we simply guess
                   a suitable buffer size here. */
                char estr[1024]; /* big enough!?! */
                if ( strerror_r( eno, estr, sizeof estr ) )
                    snprintf( estr, sizeof estr, "Error %d occurred", eno );
                int elen = strlen( estr );
                while ( m )
                {
                    char xfmt[(m - f) + elen + 1];
                    sprintf( xfmt, "%.*s%s", (int)(m - f), f, estr );
                    vfprintf( cfg.file, xfmt, arglist );
                    f = m + 2;
                    m = strstr( f, "%m" );
                }
            }
            vfprintf( cfg.file, f, arglist );
            fflush( cfg.file );
        }
    }
    UNLOCK_LOG();
    return;
}

void logprintf( int pri, const char *fmt, ... )
{
    va_list arglist;

    va_start( arglist, fmt );
    vlogprintf( pri, fmt, arglist );
    va_end( arglist );
    return;
}

/* Exported, but intended to be used solely by xlogprintf() macro! */
void xlogprintf_( const char *file, const char *func, int line, int pri, const char *fmt, ... )
{
    int eno = errno;
    va_list arglist;
    int n = snprintf( NULL, 0, "(%s:%s:%d) %s", file, func, line, fmt );
    char xfmt[n+1];

    snprintf( xfmt, sizeof xfmt, "(%s:%s:%d) %s", file, func, line, fmt );
    errno = eno;
    va_start( arglist, fmt );
    vlogprintf( pri, xfmt, arglist );
    va_end( arglist );
    return;
}


#ifdef LOGPRINTF_TEST
int main( int argc, char *argv[] )
{
    xlogprintf( LOG_DEBUG, "roedel %s\n", "farz" );
    logprintf_init( LOG_DEBUG, argv[0], LOG_TO_FILE | LOG_TO_SYSLOG, stderr );
    for ( int a = 1; a < argc; ++a )
        logprintf( LOG_INFO, "%s\n", argv[a] );
    fopen( "-", "r" );
    xlogprintf( LOG_ERR, "fopen(%s,%s): %m\n", "\"-\"", "\"r\"" );
#line 665 "fake.cxx"
    errno = 12345;
    xlogprintf( LOG_DEBUG, "%m %m\n" );
    return 0;
}
#endif /* LOGPRINTF_TEST */

/* EOF */
