/*
 * message.h
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


#ifndef MESSAGE_H_INCLUDED
#define MESSAGE_H_INCLUDED


#include <stdlib.h>
#include <stdint.h>

#include <arpa/inet.h>  /* ntohs and friends */

#include "util.h"


/* Size constants. */
#define MSG_HDR_SIZE        20
#define MSG_MAX_PAY_SIZE    65000
#define MSG_MAX_SIZE        (MSG_HDR_SIZE + MSG_MAX_PAY_SIZE)

/* Convert raw data at word-offset W from buffer P to host-order
   32-bit unsigned. */


#define ADDOFF(P,B)         (((uint8_t *)(P))+(B))

#define NTOH16(P,B)         ntohs( *(uint16_t *)ADDOFF(P,B) )
#define NTOH32(P,B)         ntohl( *(uint32_t *)ADDOFF(P,B) )

#define HTON16(V)           htons( V )
#define HTON32(V)           htonl( V )


/* Byte offsets of header fields. */
#define HDR_OFF_TYPE        0
#define HDR_OFF_PAYLEN      2
#define HDR_OFF_SRCID       4
#define HDR_OFF_DSTID       8
#define HDR_OFF_TRID        12
#define HDR_OFF_EXID        14
#define HDR_OFF_FLAGS       16

/* Macros to get individual header fields from a raw message. */
#define HDR_GET_TYPE(P)     NTOH16(P,HDR_OFF_TYPE)
#define HDR_GET_PAYLEN(P)   NTOH16(P,HDR_OFF_PAYLEN)
#define HDR_GET_SRCID(P)    NTOH32(P,HDR_OFF_SRCID)
#define HDR_GET_DSTID(P)    NTOH32(P,HDR_OFF_DSTID)
#define HDR_GET_TRID(P)     NTOH16(P,HDR_OFF_TRID)
#define HDR_GET_EXID(P)     NTOH16(P,HDR_OFF_EXID)
#define HDR_GET_FLAGS(P)    NTOH32(P,HDR_OFF_FLAGS)

/* Macros to set individual header fields from host order value. */
#define HDR_SET_TYPE(P,V)   (*(uint16_t *)ADDOFF(P,HDR_OFF_TYPE)  =(uint16_t)(V))
#define HDR_SET_PAYLEN(P,V) (*(uint16_t *)ADDOFF(P,HDR_OFF_PAYLEN)=(uint16_t)(V))
#define HDR_SET_SRCID(P,V)  (*(uint32_t *)ADDOFF(P,HDR_OFF_SRCID) =(uint32_t)(V))
#define HDR_SET_DSTID(P,V)  (*(uint32_t *)ADDOFF(P,HDR_OFF_DSTID) =(uint32_t)(V))
#define HDR_SET_TRID(P,V)   (*(uint16_t *)ADDOFF(P,HDR_OFF_TRID)  =(uint16_t)(V))
#define HDR_SET_EXID(P,V)   (*(uint16_t *)ADDOFF(P,HDR_OFF_EXID)  =(uint16_t)(V))
#define HDR_SET_FLAGS(P,V)  (*(uint32_t *)ADDOFF(P,HDR_OFF_FLAGS) =(uint32_t)(V))

/* Attribute types. */
// TODO: sync with protocol.txt !!!
#define ATT_PING        0x0001
#define ATT_LIST        0x1001
#define ATT_USERID      0x1101
#define ATT_USERNAME    0x1102
#define ATT_CATALOG     0x2001
#define ATT_GET         0x2002
#define ATT_SIZE        0x2003
#define ATT_OFFSET      0x2004
#define ATT_OFFER       0x2101
#define ATT_NAME        0x2102
#define ATT_MD5         0x2104
#define ATT_DATA        0x2105


typedef
    struct MHDR_T_STRUCT
    mhdr_t;

typedef
    struct MBUF_T_STRUCT
    mbuf_t;

struct MHDR_T_STRUCT {
    uint16_t type;
    uint16_t paylen;
    uint32_t srcid;
    uint32_t dstid;
    uint16_t trid;
    uint16_t exid;
    uint32_t flags;
};

struct MBUF_T_STRUCT {
    mbuf_t *next;
    mhdr_t hdr;
    size_t bsize;
    size_t boff;
    uint8_t *b; /* Keep b the last member to preserve alignment! */
};


extern mbuf_t *mbuf_new( void );
extern mbuf_t *mbuf_resize( mbuf_t **pp, size_t size );
extern void mbuf_free( mbuf_t **p );

extern int mhdr_decode( mbuf_t *m );
extern int mhdr_encode( mbuf_t *m );

#endif /* ndef _H_INCLUDED */

/* EOF */
