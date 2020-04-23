/*
 * logprintf.h
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


#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>


/* Supported logging modes. */
#define LOG_TO_FILE     1
#define LOG_TO_SYSLOG   2


/* Priority levels (borrowed from syslog.h). */
#ifndef LOG_EMERG
    #define LOG_EMERG   0     /* system is unusable */
#endif
#ifndef LOG_ALERT
    #define LOG_ALERT   1     /* action must be taken immediately */
#endif
#ifndef LOG_CRIT
    #define LOG_CRIT    2     /* critical conditions */
#endif
#ifndef LOG_ERR
    #define LOG_ERR     3     /* error conditions */
#endif
#ifndef LOG_WARNING
    #define LOG_WARNING 4     /* warning conditions */
#endif
#ifndef LOG_NOTICE
    #define LOG_NOTICE  5     /* normal but significant condition */
#endif
#ifndef LOG_INFO
    #define LOG_INFO    6     /* informational */
#endif
#ifndef LOG_DEBUG
    #define LOG_DEBUG   7     /* debug-level messages */
#endif


/*
 Initialize the logging system configuration.

 lvl:
   cut-off priority, above which log messages are simply ignored

 id:
   constant string to prepend to log messages; if the object
   referenced ceases to exist, the behavior is undefined;
   NULL or argv[0] (the program name) are sensible choices

 mode:
   any bitwise-or'd combination of one or more of LOG_TO_FILE
   and LOG_TO_SYSLOG, or zero (logging disabled)

 fp:
   file pointer to use when logging to a file

 NOTE: Can be called repeatedly to change the configuration on
 the fly. If not called explicitly beforehand, the equivalent of
 logprintf_init( LOG_DEBUG, NULL, LOG_TO_FILE, stderr ); is
 executed exactly once upon the first logging action.

 NOTE: If desirable, syslog specific settings can be tweaked by
 *subsequently* calling the openlog() function explicitly, with
 custom parameters.
*/
void logprintf_init( int lvl, const char *id, unsigned mode, FILE *fp );


/*
 Log a formatted message at the specified priority. Supported are
 all format specifiers recognized by printf(), and additionally the
 syslog-style "%m" sequence, which consumes no argument but instead
 is replaced by a textual error description equivalent to what can
 be obtained by calling strerror_r(errno).

 NOTE: Although the %m sequence is supported in file mode as well,
 it comes at the cost of considerably increased processing effort.

 In file mode log lines follow the format "TS ID[PID]:PRI: MSG",
 where:
  TS : local time in ISO8601 format: yyyy-mm-ddTHH:MM:SS.uuuuuu±ZZ:zz
  ID : source identifier as set via logprintf_init(), or "(null)"
  PID: process ID
  PRI: priority
  MSG: formatted message

 CAVEAT: Terminating linefeeds are not added automatically in file
 mode, they have to be included in the formatted message.
*/
void logprintf( int pri, const char *fmt, ... );


/*
 Equivalent to logprintf(), except called with a va_list instead of
 a variable number of arguments.  Does not call the va_end macro.
 The value of arglist is undefined after the call.
*/
void vlogprintf( int pri, const char *fmt, va_list arglist );


/*
 Macro to log a message along with the exact source location, i.e.
 source file name, function name and number of the line the macro
 appears in. Otherwise it is equivalent to logprintf().

 The location triplet "(FILE:FUNCTION:LINE) " is prepended to the
 formatted message.

 NOTE: Calling the referenced xlogprintf_() function directly,
 although possible, is strongly discouraged!
*/
void xlogprintf_( const char *file, const char *func, int line, int pri, const char *fmt, ... );
#define xlogprintf(PRI, ...)    xlogprintf_( __FILE__, __func__, __LINE__, (PRI), __VA_ARGS__ )


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ndef _H_INCLUDED */

/* EOF */
