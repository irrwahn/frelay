/*
 * transfer.c
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

#define _POSIX_C_SOURCE 200809L     /* strdup */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <prng.h>

#include "transfer.h"
#include "util.h"


/* List of valid active offers. */
static transfer_t *offers = NULL;
static transfer_t *downloads = NULL;

// TODO:
//transfer_t *download_new( ... )


/* Add an offer to the pending list. */
transfer_t *offer_new( uint64_t dest, const char *filename )
{
    int64_t fsz;
    transfer_t *o;

    if ( 0 > ( fsz = fsize( filename ) ) )
        return NULL;
    o = malloc( sizeof *o );
    die_if( NULL == o, "malloc() failed: %m.\n" );
    o->rid = dest;
    o->oid = random();
    o->name = strdup( filename );
    o->partname = NULL;
    o->size = fsz;
    // TODO hash ?
    // TODO ttl ?
    o->offset = 0;
    o->fd = -1;
    o->act = time( NULL );
    o->next = offers;
    offers = o;
    DLOG( "Offer added: %016"PRIx64"\n", o->oid );
    return o;
}

transfer_t *offer_match( uint64_t oid, uint64_t rid )
{
    for ( transfer_t *o = offers; NULL != o; o = o->next )
        if ( oid == o->oid && rid == o->rid )
        {
            o->act = time( NULL );
            return o;
        }
    return NULL;
}

void *offer_read( transfer_t *o, uint64_t off, size_t *psz )
{
    ssize_t n;
    void *p;

    errno = 0;
    if ( 0 > o->fd )
        o->fd = open( o->name, O_RDONLY );
    if ( 0 > o->fd )
    {
        DLOG( "open(%s,O_RDONLY) failed: %m.\n", o->name );
        return NULL;
    }
    if ( (off_t)-1 == lseek( o->fd, off, SEEK_SET ) )
    {
        DLOG( "lseek() failed: %m.\n" );
        return NULL;
    }
    p = malloc( *psz );
    die_if( NULL == p, "malloc(%zu) failed: %m.\n", *psz );
    n = read( o->fd, p, *psz );
    if ( 0 > n )
    {
        DLOG( "read(%zu) failed: %m.\n", *psz );
        close( o->fd );
        o->fd = -1;
        free( p );
        p = NULL;
    }
    else if ( 0 == n )
    {
        DLOG( "read(%zu) hit EOF.\n", *psz );
        close( o->fd );
        o->fd = -1;
        free( p );
        p = NULL;
        *psz = 0;
    }
    else if ( (ssize_t)*psz > n )
    {
        DLOG( "read(%zu) fell short, gave %zd.\n", *psz, n );
        *psz = n;
    }
    return p;
}


/* Add a download to the pending list. */
transfer_t *download_new( void )
{
    transfer_t *d;

    d = malloc( sizeof *d );
    die_if( NULL == d, "malloc() failed: %m.\n" );
    memset( d, 0, sizeof *d );
    /* Other fields are filled in by caller. */
    d->offset = 0;
    d->fd = -1;
    d->act = time( NULL );
    d->next = downloads;
    downloads = d;
    return d;
}

transfer_t *download_match( uint64_t oid )
{
    for ( transfer_t *d = downloads; NULL != d; d = d->next )
        if ( oid == d->oid )
        {
            d->act = time( NULL );
            return d;
        }
    return NULL;
}

int download_write( transfer_t *d, void *data, size_t sz )
{
    ssize_t n;

    errno = 0;
    if ( 0 > d->fd )
        d->fd = open( d->partname, O_WRONLY | O_CREAT | O_EXCL, 00600 );
    if ( 0 > d->fd )
    {
        DLOG( "open(%s,O_WRONLY|O_CREAT|O_EXCL) failed: %m.\n", d->partname );
        return -1;
    }
    n = write( d->fd, data, sz );
    if ( (ssize_t)sz > n )
    {
        if ( 0 < n )
            DLOG( "write(%zu) failed: %m.\n", sz );
        else
            DLOG( "write(%zu) fell short, put %zd.\n", sz, n );
        close( d->fd );
        d->fd = -1;
        return -1;
    }
    return 0;
}

int download_resume( transfer_t *d )
{
    off_t off;
    
    errno = 0;
    if ( 0 <= d->fd )
        return -1;
    d->fd = open( d->partname, O_WRONLY );
    if ( 0 > d->fd )
    {
        DLOG( "open(%s,O_WRONLY) failed: %m.\n", d->partname );
        return -1;
    }
    off = lseek( d->fd, 0, SEEK_END );
    if ( (off_t)-1 == off )
    {
        DLOG( "lseek() failed: %m.\n" );
        close( d->fd );
        d->fd = -1;
        return -1;
    }
    d->offset = off;
    return 0;
}


/* Close all open file descriptors in offer and download lists. */
void transfer_closeall( void )
{
    transfer_t *lists[] = { offers, downloads };
    transfer_t *p;
    time_t now = time( NULL );

    for ( int i = 0; i < 2; ++i )
    {
        for ( p = lists[i]; NULL != p; p = p->next )
        {
            if ( 0 <= p->fd )
            {
                close( p->fd );
                p->fd = -1;
                p->act = now;
            }
        }
    }
    return;
}

/* Clear out the pending offer and download lists. */
void transfer_upkeep( time_t timeout )
{
    transfer_t *lists[] = { offers, downloads };
    transfer_t *p, *next, *prev;
    time_t now = time( NULL );

    for ( int i = 0; i < 2; ++i )
    {
        next = prev = NULL;
        for ( p = lists[i]; NULL != p; p = next )
        {
            next = p->next;
            if ( p->act + timeout < now )
            {   /* Entry past best before date, ditch it. */
                DLOG( "%s timed out: %016"PRIx64"\n", i ? "Download" : "Offer", p->oid );
                if ( prev )
                    prev->next = next;
                else 
                {
                    if ( i )
                        downloads = next;
                    else
                        offers = next;
                }
                free( p->name );
                p->name = NULL;
                free( p->partname );
                p->partname = NULL;
                if ( 0 <= p->fd )
                    close( p->fd );
                free( p );
            }
            else
                prev = p;
        }
    }
    return;
}


/* EOF */
