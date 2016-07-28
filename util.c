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


#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "util.h"


int set_nonblocking( int fd )
{
    int flags;

    if ( 0 > ( flags = fcntl( fd, F_GETFL ) ) )
        return -1;
    flags = (unsigned)flags | O_NONBLOCK;
    return -1 == fcntl( fd, F_SETFL, flags ) ? -1 : 0;
}

void *memdup( void *s, size_t len )
{
    void *d = malloc( len );
    die_if( NULL == d, "malloc() failed: %m.\n" );
    return memcpy( d, s, len );
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
    DLOG( "Drained %d bytes from %d.\n", r, fd );
    return r;
}
#endif


/* EOF */
