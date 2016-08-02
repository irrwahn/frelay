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

#include <ntime.h>

#include "util.h"


mbuf_t *mbuf_new( mbuf_t **pp )
{
    mbuf_t *p;

    /* We always start out just big enough to hold the header. */
    p = malloc( sizeof *p + MSG_HDR_SIZE );
    die_if( NULL == p, "malloc() failed: %m.\n" );
    p->next = NULL;
    p->bsize = MSG_HDR_SIZE;
    p->boff = 0;
    p->b = (uint8_t *)p + sizeof *p;
    if ( NULL != pp )
        *pp = p;
    //DLOG( "New buffer address: %p\n", p );
    return p;
}

void mbuf_free( mbuf_t **pp )
{
    //DLOG( "Freeing buffer address: %p\n", *pp );
    free( *pp );
    *pp = NULL;
}

mbuf_t *mbuf_resize( mbuf_t **pp, size_t paylen )
{
    /* CAVEAT: _Never_ resize an already chain-linked mbuf! */
    mbuf_t *p = *pp;

    paylen += MSG_HDR_SIZE;
    die_if( MSG_MAX_SIZE < paylen, "%d > MSG_MAX_SIZE!\n", paylen );
    p = realloc( p, sizeof *p + paylen );
    die_if( NULL == p, "realloc() failed: %m.\n" );
    //DLOG( "Resized buffer %p: %zu to %zu\n", p, p->bsize, paylen );
    p->b = (uint8_t *)p + sizeof *p;
    p->bsize = paylen;
    if ( p->boff > p->bsize )
        p->boff = p->bsize;
    return *pp = p;
}

mbuf_t *mbuf_grow( mbuf_t **pp, size_t amount )
{
    //DLOG( "Grow buffer %p by %zu\n", *pp, amount );
    mbuf_t *p = mbuf_resize( pp, (*pp)->bsize - MSG_HDR_SIZE + amount );
    memset( ADDOFF( *pp, (*pp)->bsize - amount ), 0, amount );
    return p;
}

mbuf_t *mbuf_compose( mbuf_t **pp, enum MSG_TYPE type,
                    uint64_t srcid, uint64_t dstid, uint64_t trfid )
{
    if ( NULL == *pp )
        mbuf_new( pp );
    else
        mbuf_resize( pp, 0 );
    mbuf_t *p = *pp;
    p->boff = 0;
    HDR_SET_TYPE( p, type );
    HDR_SET_PAYLEN( p, 0 );
    HDR_SET_RFU( p, 0 );
    HDR_SET_TS( p, ntime_get() );
    HDR_SET_SRCID( p, srcid );
    HDR_SET_DSTID( p, dstid );
    HDR_SET_TRFID( p, trfid );
    return p;
}

mbuf_t *mbuf_to_response( mbuf_t **pp )
{
    die_if( NULL == pp, "Need an already filled mbuf struct!\n" );
    mbuf_resize( pp, 0 );
    (*pp)->boff = 0;
    HDR_CLASS_TO_RES( *pp );
    HDR_SET_PAYLEN( *pp, 0 );
    HDR_SET_RFU( *pp, 0 );
    HDR_SET_TS( *pp, ntime_get() );
    /* swap SRCID and DSTID, keep TRID and EXID! */
    uint64_t srcid = HDR_GET_SRCID( *pp );
    HDR_SET_SRCID( *pp, HDR_GET_DSTID( *pp ) );
    HDR_SET_DSTID( *pp, srcid );
    return *pp;
}

mbuf_t *mbuf_to_error_response( mbuf_t **pp, enum SC_ENUM ec )
{
    const char *errmsg;

    mbuf_to_response( pp );
    HDR_CLASS_TO_ERR( *pp );
    errmsg = sc_msgstr( ec );
    mbuf_addattrib( pp, MSG_ATTR_ERROR, 8, (uint64_t)ec );
    mbuf_addattrib( pp, MSG_ATTR_NOTICE, strlen( errmsg ) + 1, errmsg );
    return *pp;
}


enum AVTYPE {
    AVTYPE_NONE,
    AVTYPE_UI64,
    AVTYPE_STR,
    AVTYPE_BLOB,
};

int mbuf_addattrib( mbuf_t **pp, enum MSG_ATTRIB attype, size_t length, ... )
{
    enum AVTYPE avtype = AVTYPE_NONE;
    size_t aoff;
    uint8_t *ap;
    va_list arglist;

    switch ( attype )
    {
    case MSG_ATTR_USERNAME:     avtype = AVTYPE_STR;  break;
    case MSG_ATTR_PUBKEY:       avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_CHALLENGE:    avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_DIGEST:       avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_SIGNATURE:    avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_TTL:          avtype = AVTYPE_UI64; length = 8; break;
    case MSG_ATTR_PEERID:       avtype = AVTYPE_UI64; length = 8; break;
    case MSG_ATTR_PEERNAME:     avtype = AVTYPE_STR;  break;
    case MSG_ATTR_OFFERID:      avtype = AVTYPE_UI64; length = 8; break;
    case MSG_ATTR_FILENAME:     avtype = AVTYPE_STR;  break;
    case MSG_ATTR_SIZE:         avtype = AVTYPE_UI64; length = 8; break;
    case MSG_ATTR_FILEHASH:     avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_OFFSET:       avtype = AVTYPE_UI64; length = 8; break;
    case MSG_ATTR_DATA:         avtype = AVTYPE_BLOB; break;
    case MSG_ATTR_OK:           avtype = AVTYPE_NONE; length = 0; break;
    case MSG_ATTR_ERROR:        avtype = AVTYPE_UI64; length = 8; break;
    case MSG_ATTR_NOTICE:       avtype = AVTYPE_STR;  break;
    default:
        die_if( 1, "Unrecognized attribute: 0x%04"PRIX16".\n", attype );
        break;
    }

    aoff = (*pp)->bsize;
    mbuf_grow( pp, ROUNDUP8( length ) + 8 );
    HDR_SET_PAYLEN( (*pp), (*pp)->bsize - MSG_HDR_SIZE );
    ap = (*pp)->b + aoff;
    *(uint16_t *)(ap + 0) = HTON16( attype );
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
            if ( 0 < length )
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

int mbuf_getnextattrib( mbuf_t *p, enum MSG_ATTRIB *ptype, size_t *plen, void **pval )
{
    if ( p->bsize < p->boff + 8 )
    {
    ERR:
        *ptype = MSG_ATTR_INVALID;
        *plen  = 0;
        *pval  = NULL;
        return -1;
    }
    *plen = NTOH16( *(uint16_t *)ADDOFF( p, p->boff + 2 ) );
    if ( p->bsize < p->boff + 8 + *plen )
        goto ERR;
    *ptype = NTOH16( *(uint16_t *)ADDOFF( p, p->boff ) );
    *pval  = ADDOFF( p, p->boff + 8 );
    p->boff = p->boff + 8 + ROUNDUP8( *plen );
    return 0;
}

int mbuf_resetgetattrib( mbuf_t *p )
{
    p->boff = MSG_HDR_SIZE;
    return 0;
}

const char *mtype2str( int mtype )
{
    switch ( MTYPE_CUT_CLASS( mtype ) )
    {
    case MTYPE_REGISTER: return "REGISTER";  break;
    case MTYPE_LOGIN:    return "LOGIN";     break;
    case MTYPE_AUTH:     return "AUTH";      break;
    case MTYPE_LOGOUT:   return "LOGOUT";    break;
    case MTYPE_PEERLIST: return "PEERLIST";  break;
    case MTYPE_OFFER:    return "OFFER";     break;
    case MTYPE_GETFILE:  return "GETFILE";   break;
    case MTYPE_PING:     return "PING";      break;
    default:
        break;
    }
    return "UNKNOWN";
}

const char *mclass2str( int mtype )
{
    switch ( MTYPE_GET_CLASS( mtype ) )
    {
    case MCLASS_IND: return "IND";  break;
    case MCLASS_REQ: return "REQ";  break;
    case MCLASS_RES: return "RES";  break;
    case MCLASS_ERR: return "ERR";  break;
    default:
        break;
    }
    return "INVALID";
}

#ifdef DEBUG
#include <inttypes.h>
extern void mbuf_dump( mbuf_t *m )
{
    uint16_t paylen = HDR_GET_PAYLEN( m );
    DLOG( "mbuf  : %p\n", m );
    DLOG( "b     : %p\n", m->b );
    DLOG( "bsize : %zu\n", m->bsize );
    DLOG( "Header:\n" );
    DLOG( "Type  : 0x%04" PRIX16"\n", HDR_GET_TYPE( m ) );
    DLOG( "Paylen: 0x%04" PRIu16"\n", paylen );
    DLOG( "Rfu   : 0x%08" PRIX32"\n", HDR_GET_RFU( m ) );
    DLOG( "Ts    : 0x%016"PRIX64"\n", HDR_GET_TS( m ) );
    DLOG( "SrcID : 0x%016"PRIX64"\n", HDR_GET_SRCID( m ) );
    DLOG( "DstID : 0x%016"PRIX64"\n", HDR_GET_DSTID( m ) );
    DLOG( "TrfID : 0x%016"PRIX64"\n", HDR_GET_TRFID( m ) );
    if ( 0 != paylen )
    {
        DLOG( "Payload:\n" );
        DLOGHEX( m->b + MSG_HDR_SIZE, paylen, 8 );
    }
}
#endif

/* EOF */
