/*
 * util.c
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

#define _POSIX_C_SOURCE  200809L

#include "util.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>


int set_nonblocking( int fd )
{
    int flags;

    if ( 0 > ( flags = fcntl( fd, F_GETFL ) ) )
        return -1;
    flags = (unsigned)flags | O_NONBLOCK;
    return fcntl( fd, F_SETFL, flags );
}

int set_cloexec( int fd )
{
    int flags;

    if ( 0 > ( flags = fcntl( fd, F_GETFL ) ) )
        return -1;
    flags = (unsigned)flags | O_CLOEXEC;
    return fcntl( fd, F_SETFL, flags );
}

int pcmd( const char *cmd, int (*cb)(const char *) )
{
    FILE *fp;
    static char buf[1000];

    if ( NULL == ( fp = popen( cmd, "r" /* "re" */ ) ) )
        return -1;
    while ( NULL != fgets( buf, sizeof buf, fp ) &&  0 <= cb( buf ) )
        continue;
    return pclose( fp );
}

void *malloc_s( size_t size )
{
    void *d = malloc( size );
    die_if( NULL == d, "malloc() failed: %m.\n" );
    return d;
}

void *realloc_s( void *p, size_t size )
{
    p = realloc( p, size );
    die_if( NULL == p, "realloc() failed: %m.\n" );
    return p;
}

void *memdup_s( void *s, size_t len )
{
    void *d = malloc_s( len );
    return memcpy( d, s, len );
}

char *strdup_s( const char *s )
{
    char *d = strdup( s );
    die_if( NULL == d, "strdup() failed: %m.\n" );
    return d;
}

char *strdupcat_s( const char *s1, const char *s2 )
{
    size_t l1 = strlen( s1 );
    char *d = malloc_s( l1 + strlen( s2 ) + 1 );
    strcpy( d, s1 );
    strcpy( d + l1, s2 );
    return d;
}

char *strdupcat2_s( const char *s1, const char *s2, const char *s3 )
{
    size_t l1 = strlen( s1 );
    size_t l2 = strlen( s2 );
    char *d = malloc_s( l1 + l2 + strlen( s3 ) + 1 );
    strcpy( d, s1 );
    strcpy( d + l1, s2 );
    strcpy( d + l1 + l2, s3 );
    return d;
}


#ifdef DEBUG
int drain_fd( int fd )
{
    int r;
    char buf[10000];

    r = read( fd, buf, sizeof buf );
    return_if( 0 > r, -1, "read(%d) tripped: %m.\n", fd );
    return_if( 0 == r, 0, "read(%d): remote station disconnected.\n", fd );
    buf[r] = '\0';
    fputs( buf, stderr );
    //DLOG( "Drained %d bytes from %d.\n", r, fd );
    return r;
}
#endif


/* EOF */
