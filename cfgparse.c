/*
 * cfgparse.c
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

/*
 *  Ye crappy config file pars0r:
 *  - plain, simple "<name>=<some string>" one-liners
 *  - no sections
 *  - no quoting
 *  - no escaping
 *  - no line continuation
 *  - no inline comments
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgparse.h"
#include "util.h"


#define LINE_MAX    4000


static inline char *skip_ws( char *s )
{
    char *p = s;
    while ( *p && isspace( (unsigned char)*p ) )
        ++p;
    return p;
}

static inline char *skip_name( char *s )
{
    char *p = s;
    while ( *p  && '=' != *p && !isspace( (unsigned char)*p ))
        ++p;
    return p;
}

static inline char *rtrim( char *s )
{
    char *p = s;
    if ( '\0' == *p )
        return p;
    p += strlen( p ) - 1;
    while ( isspace( (unsigned char)*p ) )
    {
        *p = '\0';
        if ( p == s )
            break;
        --p;
    }
    return s;
}

static inline int cfg_parse_assign( const char *name, const char *val, cfg_parse_def_t *def )
{
    for ( cfg_parse_def_t *p = def; NULL != p && NULL != p->name; ++p )
    {
        if ( 0 == strcmp( name, p->name ) )
        {
            switch ( p->type )
            {
            case CFG_PARSE_T_INT:
                *(int *)p->pvar = strtol( val, NULL, 0 );
                DLOG( "Config set: %s=%d\n", name, *(int *)p->pvar );
                break;
            case CFG_PARSE_T_STR:
                free( *(char **)p->pvar );
                *(char **)p->pvar = strdup_s( val );
                DLOG( "Config set: %s=%s\n", name, *(char **)p->pvar );
                break;
            default:
                DLOG( "Ignoring unknown config type %d\n", (int)p->type );
                break;
            }
            return 0;
        }
    }
    return -1;
}

int cfg_parse_line( char *line, cfg_parse_def_t *def )
{
    int r;
    char *name = "", *val = "";

    name = skip_ws( line );
    if ( '#' == *name || '\0' == *name )
        return 0;
    val = skip_ws( skip_name( name ) );
    if ( '=' != *val )
        return -1;
    *val++ = '\0';
    name = rtrim( name );
    if ( '\0' == *name )
        return -1;
    val = rtrim( skip_ws( val ) );
    if ( 0 != ( r = cfg_parse_assign( name, val, def ) ) )
        DLOG( "Config ignored: %s=%s\n", name, val );
    return r;
}

int cfg_parse_fp( FILE *fp, cfg_parse_def_t *def )
{
    int r = 0;
    char *line = malloc_s( LINE_MAX );

    while ( NULL != fgets( line, LINE_MAX, fp ) )
        r += cfg_parse_line( line, def );
    free( line );
    return r ? -1 : 0;
}

int cfg_parse_file( const char *filename, cfg_parse_def_t *def )
{
    int r = -1;
    FILE *fp;

    if ( NULL != ( fp = fopen( filename, "r" ) ) )
    {
        DLOG( "Reading config from '%s'\n", filename );
        r = cfg_parse_fp( fp, def );
        fclose( fp );
    }
    return r;
}

int cfg_write_fp( FILE *fp, cfg_parse_def_t *def )
{
    int r = 0;

    for ( cfg_parse_def_t *p = def; 0 <= r && NULL != p && NULL != p->name; ++p )
    {
        switch ( p->type )
        {
        case CFG_PARSE_T_INT:
            r = fprintf( fp, "%s=%d\n", p->name, *(int *)p->pvar );
            break;
        case CFG_PARSE_T_STR:
            r = fprintf( fp, "%s=%s\n", p->name, *(char **)p->pvar );
            break;
        default:
            DLOG( "Ignoring unknown config type %d\n", (int)p->type );
            break;
        }
    }
    return r < 0 ? -1 : 0;
}

int cfg_write_file( const char *filename, cfg_parse_def_t *def )
{
    int r = -1;
    FILE *fp;

    if ( NULL != ( fp = fopen( filename, "w" ) ) )
    {
        DLOG( "Writing config to '%s'\n", filename );
        r = cfg_write_fp( fp, def );
        fclose( fp );
    }
    return r;
}

/* EOF */
