/*
 * message.c
 *
 * Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

void mbuf_free( mbuf_t **pp )
{
    DLOG( "Freeing buffer address: %p\n", *pp );
    free( *pp );
    *pp = NULL;
}


int mhdr_decode( mbuf_t *m )
{
    return_if( !m || !m->b || m->boff < MSG_HDR_SIZE, -1, "Invalid parameter.\n" );
    m->hdr.type   = HDR_GET_TYPE( m->b );
    m->hdr.paylen = HDR_GET_PAYLEN( m->b );
    m->hdr.rfu    = HDR_GET_RFU( m->b );
    m->hdr.ts     = HDR_GET_TS( m->b );
    m->hdr.srcid  = HDR_GET_SRCID( m->b );
    m->hdr.dstid  = HDR_GET_DSTID( m->b );
    m->hdr.trid   = HDR_GET_TRID( m->b );
    m->hdr.exid   = HDR_GET_EXID( m->b );
    return 0;
}

int mhdr_encode( mbuf_t *m )
{
    return_if( !m || !m->b || m->bsize < MSG_HDR_SIZE, -1, "Invalid parameter.\n" );
    HDR_SET_TYPE( m->b, m->hdr.type );
    HDR_SET_PAYLEN( m->b, m->hdr.paylen );
    HDR_SET_RFU( m->b, m->hdr.rfu );
    HDR_SET_TS( m->b, m->hdr.ts );
    HDR_SET_SRCID( m->b, m->hdr.srcid );
    HDR_SET_DSTID( m->b, m->hdr.dstid );
    HDR_SET_TRID( m->b, m->hdr.trid );
    HDR_SET_EXID( m->b, m->hdr.exid );
    return 0;
}

#ifdef DEBUG
#include <inttypes.h>
extern void mhdr_dump( mbuf_t *m )
{
    DLOG( "Header:\n" );
    DLOG( "Type  : %04"PRIX16"\n", m->hdr.type );
    DLOG( "Paylen: %04"PRIX16"\n", m->hdr.paylen );
    DLOG( "Rfu   : %08"PRIX32"\n", m->hdr.rfu );
    DLOG( "Ts    : %016"PRIX64"\n", m->hdr.ts );
    DLOG( "SrcID : %016"PRIX64"\n", m->hdr.srcid );
    DLOG( "DstID : %016"PRIX64"\n", m->hdr.dstid );
    DLOG( "TrID  : %016"PRIX64"\n", m->hdr.trid );
    DLOG( "ExID  : %016"PRIX64"\n", m->hdr.exid );
    if ( 0 != m->hdr.paylen )
    {
        DLOG( "Payload:\n" );
        DLOGHEX( m->b + MSG_HDR_SIZE, m->hdr.paylen, 8 );
    }
}
#endif

/* EOF */
