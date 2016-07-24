/*
 * util.h
 *
 * Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
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


#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#ifdef DUMB_LOGGER
    #define XLOG_INIT(S)
    #define XLOG(PRI,...)   fprintf(stderr,__VA_ARGS__)
#else
    #include "logprintf.h"
    #define XLOG_INIT(S)    logprintf_init(LOG_DEBUG,(S),LOG_TO_FILE,stderr)
    #define XLOG(PRI,...)   xlogprintf((PRI),__VA_ARGS__)
#endif

#ifdef DEBUG
    #define DLOG(...)       XLOG(LOG_DEBUG,__VA_ARGS__)
    #define DLOGHEX(P,N,M) \
            do{ \
                uint8_t *p = (P); int n = (N), m = (M); \
                for ( int i = 0; i < n; i += m ) { \
                    char s[m * 3 + 1000]; \
                    for ( int j = 0; j < ( n - i >= m ? m : n - i ); ++j ) \
                        sprintf( s + j * 3, "%02X ", (unsigned)p[i+j] ); \
                    DLOG( "%04X: %s\n", i, s ); \
                } \
            }while(0);
#else
    #define DLOG(...)       do{;}while(0)
    #define DLOGHEX(P,N,M)  do{;}while(0)
#endif

#define WHOAMI()    DLOG( "<-- YOU ARE HERE. WE ARE NOT.\n" );


#define die_if(expr,...) \
            do { if ( (expr) ) { \
                XLOG( LOG_ERR, __VA_ARGS__ ); \
                XLOG( LOG_DEBUG, "Exiting after error.\n" ); \
                exit( EXIT_FAILURE ); \
            } } while(0)

#define return_if(expr,retval,...) \
            do { if ( (expr) ) { \
                XLOG( LOG_WARNING, __VA_ARGS__ ); \
                return (retval); \
            } } while(0)



extern int set_nonblocking( int fd );
#ifdef DEBUG
extern int drain_fd( int fd );
#endif


#endif /* ndef _H_INCLUDED */

/* EOF */
