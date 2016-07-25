/*
 * message.c
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

#include "message.h"

#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

#include "util.h"


mbuf_t *mbuf_new( void )
{
    mbuf_t *p;

    /* We always start out just big enough to hold the header. */
    p = malloc( sizeof *p + MSG_HDR_SIZE );
    die_if( NULL == p, "malloc() failed: %m.\n" );
    p->next = NULL;
    p->bsize = MSG_HDR_SIZE;
    p->boff = 0;
    p->b = (uint8_t *)p + sizeof *p;
    DLOG( "New buffer address: %p\n", p );
    return p;
}

void mbuf_free( mbuf_t **pp )
{
    DLOG( "Freeing buffer address: %p\n", *pp );
    free( *pp );
    *pp = NULL;
}

mbuf_t *mbuf_resize( mbuf_t **pp, size_t paylen )
{
    /* CAVEAT: _Never_ resize an already chain-linked mbuf! */
    mbuf_t *p = *pp;

    die_if( MSG_MAX_SIZE < MSG_HDR_SIZE + paylen,
            "%d > MSG_MAX_SIZE!\n", paylen );
    p = realloc( p, sizeof *p + MSG_HDR_SIZE + paylen );
    die_if( NULL == p, "realloc() failed: %m.\n" );
    p->b = (uint8_t *)p + sizeof *p;
    p->bsize = MSG_HDR_SIZE + paylen;
    if ( p->boff > p->bsize )
        p->boff = p->bsize;
    DLOG( "Resized buffer address: %p\n", p );
    return *pp = p;
}

mbuf_t *mbuf_grow( mbuf_t **pp, size_t amount )
{
    return mbuf_resize( pp, (*pp)->bsize + amount );
}

enum AVTYPE {
    AVTYPE_NONE,
    AVTYPE_UI64,
    AVTYPE_STR,
    AVTYPE_BLOB,
};

int mbuf_addattrib( mbuf_t **pp, enum MSG_ATTRIB attrib, size_t length, ... )
{
    enum AVTYPE avtype = AVTYPE_NONE;
    size_t aoff;
    uint8_t *ap;
    va_list arglist;
    
    switch ( attrib )
    {
    case MSG_ATTR_USERNAME:     avtype = AVTYPE_STR;  break;
    case MSG_ATTR_PUBKEY:       avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_CHALLENGE:    avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_DIGEST:       avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_SIGNATURE:    avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_TTL:          avtype = AVTYPE_UI64; break;
    case MSG_ATTR_PEERID:       avtype = AVTYPE_UI64; break;
    case MSG_ATTR_PEERNAME:     avtype = AVTYPE_STR;  break;
    case MSG_ATTR_OFFERID:      avtype = AVTYPE_UI64; break;
    case MSG_ATTR_FILENAME:     avtype = AVTYPE_STR;  break;
    case MSG_ATTR_SIZE:         avtype = AVTYPE_UI64; break;
    case MSG_ATTR_MD5:          avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_OFFSET:       avtype = AVTYPE_UI64; break;
    case MSG_ATTR_DATA:         avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_OK:           avtype = AVTYPE_NONE; break;
    case MSG_ATTR_ERROR:        avtype = AVTYPE_UI64; break;
    case MSG_ATTR_NOTICE:       avtype = AVTYPE_STR;  break;
    default:
        die_if( 1, "Unrecognized attribute: 0x%04"PRIx16".\n", attrib );
        break;
    }
    aoff = (*pp)->bsize;
    mbuf_grow( pp, ROUNDUP8( length ) );
    ap = (*pp)->b + aoff;
    *(uint16_t *)(ap + 0) = HTON16( attrib );
    *(uint16_t *)(ap + 2) = HTON16( length );
    *(uint32_t *)(ap + 4) = HTON32( 0 );
    va_start( arglist, length );
    switch ( avtype )
    {
    case AVTYPE_UI64: 
        {
            uint64_t v = va_arg( arglist, uint64_t );
            *(uint64_t *)(ap + 8) = HTON64( v );
        }
        break;
    case AVTYPE_STR:
        {
            char *v = va_arg( arglist, char * );
            strcpy( (char *)(ap + 8), v );
        }
        break;
    case AVTYPE_BLOB:
        {
            uint8_t *v = va_arg( arglist, uint8_t * );
            memcpy( ap + 8, v, length );
        }
        break;
    case AVTYPE_NONE:
        break;
    default:
        break;
    }
    va_end( arglist );
    return 0;
}


#ifdef DEBUG
#include <inttypes.h>
extern void mhdr_dump( mbuf_t *m )
{
    uint16_t paylen = HDR_GET_PAYLEN( m );
    DLOG( "Header:\n" );
    DLOG( "Type  : %04" PRIX16"\n", HDR_GET_TYPE( m ) );
    DLOG( "Paylen: %04" PRIX16"\n", paylen );
    DLOG( "Rfu   : %08" PRIX32"\n", HDR_GET_RFU( m ) );
    DLOG( "Ts    : %016"PRIX64"\n", HDR_GET_TS( m ) );
    DLOG( "SrcID : %016"PRIX64"\n", HDR_GET_SRCID( m ) );
    DLOG( "DstID : %016"PRIX64"\n", HDR_GET_DSTID( m ) );
    DLOG( "TrID  : %016"PRIX64"\n", HDR_GET_TRID( m ) );
    DLOG( "ExID  : %016"PRIX64"\n", HDR_GET_EXID( m ) );
    if ( 0 != paylen )
    {
        DLOG( "Payload:\n" );
        DLOGHEX( m->b + MSG_HDR_SIZE, paylen, 8 );
    }
}
#endif

/* EOF */
