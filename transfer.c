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
    o->size = fsz;
    // TODO hash ?
    // TODO ttl ?
    o->offset = 0;
    o->fp = NULL;
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

void *offer_read( transfer_t *o, uint64_t off, size_t sz )
{
    size_t n;
    void *p;

    errno = 0;
    if ( NULL == o->fp )
        o->fp = fopen( o->name, "rb" );
    if ( NULL == o->fp )
    {
        DLOG( "fopen(%s,rb) failed: %m.\n", o->name );
        return NULL;
    }
    if ( 0 != fseeko( o->fp, off, SEEK_SET ) )
    {
        DLOG( "fseeko() failed: %m.\n" );
        return NULL;
    }
    p = malloc( sz );
    die_if( NULL == p, "malloc(%zu) failed: %m.\n", sz );
    n = fread( p, sz, 1, o->fp );
    if ( n != 1 )
    {
        DLOG( "fread(%zu) fell short.\n", sz );
        if ( ferror( o->fp ) )
            DLOG( "fread(%zu) failed: %m.\n", sz );
        fclose( o->fp );
        o->fp = NULL;
        free( p );
        p = NULL;
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
    /* Other fields are be filled in by caller */
    d->offset = 0;
    d->fp = NULL;
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
    size_t n;

    errno = 0;
    if ( NULL == d->fp )
        d->fp = fopen( d->name, "wb" );
    if ( NULL == d->fp )
    {
        DLOG( "fopen(%s,wb) failed: %m.\n", d->name );
        return -1;
    }
    n = fwrite( data, sz, 1, d->fp );
    if ( n != 1 )
    {
        DLOG( "fwrite(%zu) fell short.\n", sz );
        if ( ferror( d->fp ) )
            DLOG( "fwrite(%zu) failed: %m.\n", sz );
        fclose( d->fp );
        d->fp = NULL;
        return -1;
    }
    return 0;
}


/* Clear out the pending offer and download lists. */
void transfer_upkeep( time_t timeout )
{
    transfer_t *lists[] = { offers, downloads, NULL };
    transfer_t *p, *next, *prev;
    time_t now = time( NULL );

    for ( int i = 0; lists[i]; ++i )
    {
        next = prev = NULL;
        for ( p = lists[i]; NULL != p; p = next )
        {
            next = p->next;
            if ( p->act + timeout < now )
            {   /* Entry past best before date, ditch it. */
                DLOG( "Offer timed out: %016"PRIx64"\n", p->oid );
                if ( prev )
                    prev->next = next;
                else
                    offers = next;
                free( p->name );
                if ( NULL != p->fp )
                    fclose( p->fp );
                free( p );
            }
            else
                prev = p;
        }
    }
    return;
}


/* EOF */
