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


/* Sigh. There's a POSIX standard for everything except the most useful 
things. The following is a cheap attempt at making the code at least 
somewhat portable. It however reportedly breaks on Android, which 
defines __linux__ but provides the OpenBSD API. Go figure.
*/
#define _DEFAULT_SOURCE
#if defined(__linux__)
    #include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    #include <sys/endian.h>
#elif defined(__OpenBSD__)
    #include <sys/types.h>
    #define be16toh(x) betoh16(x)
    #define be32toh(x) betoh32(x)
    #define be64toh(x) betoh64(x)
#endif

#include <stdint.h>
#include <stdlib.h>

#include "util.h"

/* Size constants. */
#define MSG_HDR_SIZE        48
#define MSG_MAX_PAY_SIZE    65400
#define MSG_MAX_SIZE        (MSG_HDR_SIZE + MSG_MAX_PAY_SIZE)


/* Network byte <--> host byte order conversion. */
#define ADDOFF(P,B)         (((uint8_t *)(P))+(B))
#define NTOH16(P,B)         be16toh( *(uint16_t *)ADDOFF(P,B) )
#define NTOH32(P,B)         be32toh( *(uint32_t *)ADDOFF(P,B) )
#define NTOH64(P,B)         be64toh( *(uint64_t *)ADDOFF(P,B) )
#define HTON16(V)           htobe16( V )
#define HTON32(V)           htobe32( V )
#define HTON64(V)           htobe64( V )


/* Byte offsets of header fields. */
#define HDR_OFF_TYPE        0
#define HDR_OFF_PAYLEN      2
#define HDR_OFF_RFU         4       
#define HDR_OFF_TS          8
#define HDR_OFF_SRCID       16
#define HDR_OFF_DSTID       24
#define HDR_OFF_TRID        32
#define HDR_OFF_EXID        40

/* Macros to get individual header fields from a raw message. */
#define HDR_GET_TYPE(P)     NTOH16(P,HDR_OFF_TYPE)
#define HDR_GET_PAYLEN(P)   NTOH16(P,HDR_OFF_PAYLEN)
#define HDR_GET_RFU(P)      NTOH32(P,HDR_OFF_RFU)
#define HDR_GET_TS(P)       NTOH64(P,HDR_OFF_TS)
#define HDR_GET_SRCID(P)    NTOH64(P,HDR_OFF_SRCID)
#define HDR_GET_DSTID(P)    NTOH64(P,HDR_OFF_DSTID)
#define HDR_GET_TRID(P)     NTOH64(P,HDR_OFF_TRID)
#define HDR_GET_EXID(P)     NTOH64(P,HDR_OFF_EXID)

/* Macros to set individual header fields in a raw message. */
#define HDR_SET_TYPE(P,V)   (*(uint16_t *)ADDOFF(P,HDR_OFF_TYPE)  =(uint16_t)(V))
#define HDR_SET_PAYLEN(P,V) (*(uint16_t *)ADDOFF(P,HDR_OFF_PAYLEN)=(uint16_t)(V))
#define HDR_SET_RFU(P,V)    (*(uint32_t *)ADDOFF(P,HDR_OFF_RFU)   =(uint32_t)(V))
#define HDR_SET_TS(P,V)     (*(uint64_t *)ADDOFF(P,HDR_OFF_TS)    =(uint64_t)(V))
#define HDR_SET_SRCID(P,V)  (*(uint64_t *)ADDOFF(P,HDR_OFF_SRCID) =(uint64_t)(V))
#define HDR_SET_DSTID(P,V)  (*(uint64_t *)ADDOFF(P,HDR_OFF_DSTID) =(uint64_t)(V))
#define HDR_SET_TRID(P,V)   (*(uint64_t *)ADDOFF(P,HDR_OFF_TRID)  =(uint64_t)(V))
#define HDR_SET_EXID(P,V)   (*(uint64_t *)ADDOFF(P,HDR_OFF_EXID)  =(uint64_t)(V))

/* Message types. */
// TODO

/* Attribute types. */
// TODO



typedef
    struct MHDR_T_STRUCT
    mhdr_t;

typedef
    struct MBUF_T_STRUCT
    mbuf_t;

struct MHDR_T_STRUCT {
    uint16_t type;
    uint16_t paylen;
    uint32_t rfu;
    uint64_t ts;
    uint64_t srcid;
    uint64_t dstid;
    uint64_t trid;
    uint64_t exid;
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

#ifdef DEBUG
extern void mhdr_dump( mbuf_t *m );
#endif

#endif /* ndef _H_INCLUDED */

/* EOF */
