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

#define _FILE_OFFSET_BITS 64    /* Tentative: Should work on most POSIX systems. */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <prng.h>

#include "transfer.h"
#include "util.h"


/* List of valid active offers. */
static transfer_t *transfers = NULL;


/**********************************************
 * Helper.
 *
 */

int64_t fsize( const char *filename )
{
    struct stat st;
    return 0 == stat( filename, &st ) ? st.st_size : -1;
}

int make_filenames( transfer_t *d, const char *s, int no_clobber )
{
    if ( no_clobber )
    {
        int fd0 = -1, fd1 = -1;
        const char *e = strrchr( s, '.' );
        int extpos = e ? e - s : (int)strlen( s );
        char *name = malloc_s( strlen( s ) + 21 );
        char *partname = malloc_s( strlen( s ) + 25 );

        strcpy( name, s );
        sprintf( partname, "%s.part", name );
        for ( int i = 1; i < INT_MAX; ++i )
        {
            sprintf( name, "%.*s.%d%s", extpos, s, i, s + extpos );
            sprintf( partname, "%s.part", name );
            if ( 0 > ( fd0 = open( name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 00600 ) ) )
                continue;
            if ( 0 > ( fd1 = open( partname, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 00600 ) ) )
            {
                unlink( name );
                close( fd0 );
                continue;
            }
            break;
        }
        close( fd0 );
        d->fd = fd1;
        d->name = name;
        d->partname = partname;
    }
    else
    {
        d->name = strdup_s( s );
        d->partname = strdupcat_s( d->name, ".part" );
    }
    return 0;
}


/**********************************************
 * Formatting.
 *
 */

static char *human_size( uint64_t sz )
{
    int m = 0;
    double f = sz;
    static char buf[30];
    static char *pfx[] = { "", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei" };

    while ( 1024.0 < f )
        f /= 1024.0, ++m;
    snprintf( buf, sizeof buf, "%.2f%sB", f, pfx[m] );
    return buf;
}

static char *human_timespan( time_t t )
{
    int n = 0;
    int s, m, h, d;
    static char buf[100];

    s = t % 60; t /= 60;
    m = t % 60; t /= 60;
    h = t % 24; t /= 24;
    d = t;
    if ( d )  n += snprintf( buf + n, sizeof buf - n, "%dd", d );
    if ( h )  n += snprintf( buf + n, sizeof buf - n, "%dh", h );
    if ( m )  n += snprintf( buf + n, sizeof buf - n, "%dm", m );
    if ( s || m + h + d == 0 )
        n += snprintf( buf + n, sizeof buf - n, "%ds", s );
    return buf;
}

char *transfer_itostr( char *buf, size_t size, const char *fmt, const transfer_t *t )
{
    int n = 0;
    time_t now = time( NULL );

    if ( NULL == fmt )
        fmt = "%i '%n' %o/%s %d %a";
    for ( const char *p = fmt; '\0' != *p; ++p )
    {
        if ( '%' != *p )
        {
            n += snprintf( buf + n, size - n, "%c", *p );
            continue;
        }
        switch( *++p )
        {
        case 'i':
            n += snprintf( buf + n, size - n, "%c,%016"PRIx64",%016"PRIx64,
                        t->type ? 'D' : 'O', t->rid, t->oid );
            break;
        case 'n':
            n += snprintf( buf + n, size - n, "%s", t->name );
            break;
        case 'N':
            n += snprintf( buf + n, size - n, "%s", t->partname );
            break;
        case 'o':
            n += snprintf( buf + n, size - n, "%"PRIu64"B", t->offset );
            break;
        case 'O':
            n += snprintf( buf + n, size - n, "%s", human_size( t->offset ) );
            break;
        case 's':
            n += snprintf( buf + n, size - n, "%"PRIu64"B", t->size );
            break;
        case 'S':
            n += snprintf( buf + n, size - n, "%s", human_size( t->size ) );
            break;
        case 'd':
            n += snprintf( buf + n, size - n, "%"PRIu64"s", (uint64_t)(now - t->tstart) );
            break;
        case 'D':
            n += snprintf( buf + n, size - n, "%s", human_timespan( now - t->tstart ) );
            break;
        case 'a':
            n += snprintf( buf + n, size - n, "%"PRIu64"s", (uint64_t)(now - t->tact) );
            break;
        case 'A':
            n += snprintf( buf + n, size - n, "%s", human_timespan( now - t->tact ) );
            break;
        case 't':
            n += snprintf( buf + n, size - n, "%"PRIu64"s", (uint64_t)(t->tstart) );
            break;
        case 'T':
            {
                char tbuf[30];
                struct tm tm;
                localtime_r( &t->tstart, &tm );
                strftime( tbuf, sizeof tbuf, "%Y-%m-%dT%H:%M:%S%z", &tm );
                n += snprintf( buf + n, size - n, "%s", tbuf );
            }
            break;
        default:
            n += snprintf( buf + n, size - n, "%c", *p );
            break;
        }

    }
    return buf;
}

int transfer_strtoi( const char *s, enum transfer_type *ptype, uint64_t *prid, uint64_t *poid )
{
    int n;
    char t = '\0';
    uint64_t r = 0ULL, o = 0ULL;

    if ( 3 == ( n = sscanf( s, " %c,%"SCNx64",%"SCNx64" ", &t, &r, &o ) )
      || 2 == ( n = sscanf( s, " %"SCNx64",%"SCNx64" ", &r, &o ) )
      || 1 == ( n = sscanf( s, " %"SCNx64" ", &o ) ) )
    {
        if ( 3 == n && NULL != ptype )
        {
            t = toupper( (unsigned char)t );
            if ( 'D' == t )
                *ptype = TTYPE_DOWNLOAD;
            else if ( 'O' == t )
                *ptype = TTYPE_OFFER;
            else
                return -1;
        }
        if ( NULL != prid ) *prid = r;
        if ( NULL != poid ) *poid = o;
        return n;
    }
    return -1;
}


/**********************************************
 * General transfer handling.
 *
 */

transfer_t *transfer_new( enum transfer_type type )
{
    transfer_t *t;

    t = malloc_s( sizeof *t );
    t->type = type;

    t->rid = t->oid = 0ULL;
    t->name = t->partname = NULL;
    t->size = t->offset = 0ULL;
    t->fd = -1;
    t->tstart = t->tact = time( NULL );
    t->next = transfers;
    transfers = t;
    return t;
}

transfer_t *transfer_match( enum transfer_type type, uint64_t rid, uint64_t oid )
{
    for ( transfer_t *t = transfers; NULL != t; t = t->next )
    {
        if ( type == t->type && oid == t->oid && rid == t->rid && 0 < t->tact )
        {
            t->tact = time( NULL );
            return t;
        }
    }
    return NULL;
}

/* Close all open file descriptors in offer and download lists. */
void transfer_closeall( void )
{
    for ( transfer_t *p = transfers; NULL != p; p = p->next )
    {
        if ( 0 <= p->fd )
        {
            close( p->fd );
            p->fd = -1;
        }
    }
    return;
}

int transfer_invalidate( transfer_t *p )
{
    p->rid = p->oid = 0ULL;
    p->tact = 0;
    close( p->fd );
    p->fd = -1;
    return 0;
}

/* Clear out the pending offer and download lists. */
void transfer_upkeep( time_t timeout )
{
    time_t now = time( NULL );

    for ( transfer_t *next = NULL, *prev = NULL, *p = transfers; NULL != p; p = next )
    {
        next = p->next;
        if ( p->tact + timeout < now )
        {   /* Entry past best before date, ditch it. */
            if ( p->rid != 0 && p->oid != 0 )
                DLOG( "%s timed out: %016"PRIx64"\n", p->type ? "Download" : "Offer", p->oid );
            else
                DLOG( "Clean out finished %s\n", p->type ? "Download" : "Offer" );
            if ( prev )
                prev->next = next;
            else
                transfers = next;
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
    return;
}

int transfer_list( int (*cb)(transfer_t *) )
{
    int n = 0;

    for ( transfer_t *p = transfers; NULL != p; p = p->next )
    {
        if ( 0ULL != p->rid && 0ULL != p->oid && 0 != p->tact )
        {
            if ( 0 > cb( p ) )
                break;
            ++n;
        }
    }
    return n;
}

int transfer_remove( const char *s )
{
    const char ti[] = "OD";
    char t, rids[17], oids[17];
    uint64_t rid, oid;
    int n = 0;

    if ( 3 != sscanf( s, " %c,%16[*0-9a-fA-F],%16[*0-9a-fA-F] ", &t, rids, oids ) )
        return -1;
    t = toupper( (unsigned char)t );

    if ( ( '*' != t && 'O' != t && 'D' != t )
      || ( strchr( rids, '*' ) && 1 != strlen( rids ) )
      || ( strchr( oids, '*' ) && 1 != strlen( oids ) ) )
        return -1;
    oid = strtoull( oids, NULL, 16 );
    rid = strtoull( rids, NULL, 16 );
    for ( transfer_t *p = transfers; NULL != p; p = p->next )
    {
        if ( ( '*' == t || ti[p->type] == t )
          && ( '*' == *rids || rid == p->rid )
          && ( '*' == *oids || oid == p->oid ) )
        {   /* We actually just invalidate the entry and leave the gory
             * list mangling to transfer_upkeep(). */
            DLOG( "%s invalidated: %c,%016"PRIx64",%016"PRIx64"\n", s, ti[p->type], p->rid, p->oid );
            transfer_invalidate( p );
            ++n;
        }
    }
    return n;
}


/**********************************************
 * Offer specific.
 *
 */

transfer_t *offer_new( uint64_t dest, const char *filename )
{
    int64_t fsz;
    transfer_t *o;

    if ( 0 > ( fsz = fsize( filename ) ) )
        return NULL;
    o = transfer_new( TTYPE_OFFER );
    o->rid = dest;
    o->oid = prng_random();
    o->name = strdup_s( filename );
    o->size = fsz;
    DLOG( "Offer added: %016"PRIx64"\n", o->oid );
    return o;
}

void *offer_read( transfer_t *o, uint64_t off, uint64_t *psz )
{
    size_t sz = *psz;
    ssize_t n;
    void *p;

    errno = 0;
    if ( 0 > o->fd )
        o->fd = open( o->name, O_RDONLY | O_CLOEXEC );
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
    p = malloc_s( sz );
    n = read( o->fd, p, sz );
    if ( 0 > n )
    {
        DLOG( "read(%zu) failed: %m.\n", sz );
        close( o->fd );
        o->fd = -1;
        free( p );
        p = NULL;
    }
    else if ( 0 == n )
    {
        DLOG( "read(%zu) hit EOF.\n", sz );
        close( o->fd );
        o->fd = -1;
        free( p );
        p = NULL;
        *psz = 0;
    }
    else if ( (ssize_t)sz > n )
    {
        DLOG( "read(%zu) fell short, gave %zd.\n", sz, n );
        *psz = n;
    }
    return p;
}


/**********************************************
 * Download specific.
 *
 */

transfer_t *download_new( void )
{
    transfer_t *d = transfer_new( TTYPE_DOWNLOAD );
    /* Info filled in by caller. */
    return d;
}

int download_write( transfer_t *d, void *data, size_t sz )
{
    ssize_t n;

    errno = 0;
    if ( 0 > d->fd )
        d->fd = open( d->partname, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 00600 );
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
    d->fd = open( d->partname, O_WRONLY | O_CLOEXEC );
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


/* EOF */
