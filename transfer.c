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


// TODO:
//static transfer_t downloads = NULL;
//transfer_t *download_new( ... )
//download_upkeep()


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
    o->offset = 0;
    // TODO md5 ?
    // TODO ttl ?
    o->fp = NULL;
    o->act = time( NULL );
    o->next = offers;
    offers = o;
    DLOG( "Offer added: %016"PRIx64"\n", o->oid );
    return o;
}

/* clear out the pending request and offer lists. */
void offer_upkeep( time_t timeout )
{
    transfer_t *o, *next, *prev;
    time_t now = time( NULL );

    next = prev = NULL;
    for ( o = offers; NULL != o; o = next )
    {
        next = o->next;
        if ( o->act + timeout < now )
        {   /* Offer passed best before date, ditch it. */
            DLOG( "Offer timed out: %016"PRIx64"\n", o->oid );
            if ( prev )
                prev->next = next;
            else
                offers = next;
            if ( NULL != o->fp )
                fclose( o->fp );
            free( o );
        }
        else
            prev = o;
    }
    return;
}


/* EOF */
