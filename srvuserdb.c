/*
 * srvuserdb.c
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

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <regex.h>

#include "srvuserdb.h"
#include "util.h"


#define UNAME_RE "^[a-zA-Z_0-9]{3,31}$"
#define DELIM    '|'

static udb_t *userdb = NULL;
static char *userdb_path = NULL;
static uint64_t next_id = 1ULL;

static int udb_nameisvalid( const char *s )
{
    static int re_isinit = 0;
    static regex_t re;

    if ( !re_isinit )
    {
        int r;
        r = regcomp( &re, UNAME_RE, REG_EXTENDED | REG_NOSUB | REG_NEWLINE );
        die_if( 0 != r, "regcomp failed.\n" );
        re_isinit = 1;
    }
    return regexec( &re, s, 0, NULL, 0 );
}

static const udb_t *udb_lookupname_( const char *s )
{
    for ( udb_t *p = userdb; NULL != p; p = p->next )
        if ( 0 == strcmp( s, p->name ) )
            return p;
    return NULL;
}

static const udb_t *udb_addentry_( uint64_t id, const char *name, const char *key )
{
    udb_t *p;

    if ( NULL == name || NULL == key || 0 != udb_nameisvalid( name ) )
        return errno = EINVAL, NULL;
    if ( NULL != udb_lookupname_( name ) )
        return errno = EEXIST, NULL;
    p = malloc_s( sizeof *p );
    p->name = strdup_s( name );
    p->key = strdup_s( key );
    p->id = id ? id : next_id;
    if ( p->id >= next_id )
        next_id = p->id + 1;
    p->next = userdb;
    userdb = p;
    DLOG( "Added user: %016"PRIX64"|%s|%s\n", id, name, key );
    return p;
}

static int udb_dropentry_( const char *name )
{
    udb_t *p = userdb, *this, *prev = NULL;

    while ( NULL != p )
    {
        this = p;
        p = p->next;
        if ( 0 == strcmp( name, this->name ) )
        {
            if ( NULL == prev )
                userdb = p;
            else
                prev->next = p;
            free( this->name );
            free( this->key );
            free( this );
            return 0;
        }
        prev = this;
    }
    return -1;
}

static int udb_load( const char *path )
{
    FILE *fp;
    static char line[16000];
    uint64_t id;
    char *name;
    char *key;

    die_if( NULL == path, "User db path not initialized!\n" );
    if ( NULL != userdb )
    {
        DLOG( "userdb already loaded.\n" );
        return 0;
    }
    if ( NULL == ( fp = fopen( path, "r" ) ) )
    {
        XLOG( LOG_WARNING, "Error opening user db '%s': %m\n", path );
        return -1;
    }
    while ( NULL != fgets( line, sizeof line, fp ) )
    {
        id = strtoull( line, &name, 16 );
        if ( 0ULL == id || DELIM != *name || NULL == ( key = strchr( ++name, '|' ) ) )
        {
            XLOG( LOG_WARNING, "Ignoring invalid line in userdb '%s': '%s'", path, line );
            continue;
        }
        *key++ = '\0';
        char *p;
        if ( NULL != ( p = strchr( key, '\r' ) ) || NULL != ( p = strchr( key, '\n' ) ) )
            *p = '\0';
        udb_addentry_( id, name, key );
    }
    fclose( fp );
    return 0;
}

static int udb_save( const char *path )
{
    FILE *fp;

    die_if( NULL == path, "User db path not initialized!\n" );
    DLOG( "Save user db as '%s'.\n", path );
    if ( NULL == ( fp = fopen( path, "w" ) ) )
    {
        XLOG( LOG_ERR, "Error opening user db '%s': %m\n", path );
        return -1;
    }
    // TODO: write to tmpfile first, then rename
    for ( udb_t *p = userdb; p; p = p->next )
    {
        fprintf( fp, "%016"PRIx64"%c%s%c%s\n", p->id, DELIM, p->name, DELIM, p->key );
        // TODO: error checking
    }
    fclose( fp );
    return 0;
}

int udb_init( const char *dbpath )
{
    free( userdb_path );
    userdb_path = strdup_s( dbpath );
    return udb_load( userdb_path );
}

const udb_t *udb_lookupname( const char *s )
{
    udb_load( userdb_path );
    return udb_lookupname_( s );
}

const udb_t *udb_addentry( uint64_t id, const char *name, const char *key )
{
    const udb_t *u;
    udb_load( userdb_path );
    u = udb_addentry_( id, name, key );
    udb_save( userdb_path );
    return u;
}

int udb_dropentry( const char *name )
{
    int r;
    udb_load( userdb_path );
    if ( 0 == ( r = udb_dropentry_( name ) ) )
        udb_save( userdb_path );
    return r;
}

uint64_t udb_gettempid( void )
{
    udb_load( userdb_path );
    return next_id++;
}


/* EOF */
