/*
 * message.h
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


#ifndef MESSAGE_H_INCLUDED
#define MESSAGE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

#include "bendian.h"
#include "util.h"

/* Round up to the next higher multiple of 8 */
#define ROUNDUP8(N)     (((N)+7)/8*8)

/* Size constants. */
#define MSG_HDR_SIZE        48
#define MSG_MAX_PAY_SIZE    65400
#define MSG_MAX_SIZE        (MSG_HDR_SIZE + MSG_MAX_PAY_SIZE)

/* Byte offsets of header fields. */
#define HDR_OFF_TYPE        0
#define HDR_OFF_PAYLEN      2
#define HDR_OFF_RFU         4
#define HDR_OFF_TS          8
#define HDR_OFF_SRCID       16
#define HDR_OFF_DSTID       24
#define HDR_OFF_TRID        32
#define HDR_OFF_EXID        40

#define ADDOFF(P,B)         (((uint8_t *)((P)->b))+(B))

/* Macros to get individual header fields from a raw message. */
#define HDR_GET_TYPE(P)     NTOH16(*(uint16_t *)ADDOFF(P,HDR_OFF_TYPE))
#define HDR_GET_PAYLEN(P)   NTOH16(*(uint16_t *)ADDOFF(P,HDR_OFF_PAYLEN))
#define HDR_GET_RFU(P)      NTOH32(*(uint32_t *)ADDOFF(P,HDR_OFF_RFU))
#define HDR_GET_TS(P)       NTOH64(*(uint64_t *)ADDOFF(P,HDR_OFF_TS))
#define HDR_GET_SRCID(P)    NTOH64(*(uint64_t *)ADDOFF(P,HDR_OFF_SRCID))
#define HDR_GET_DSTID(P)    NTOH64(*(uint64_t *)ADDOFF(P,HDR_OFF_DSTID))
#define HDR_GET_TRID(P)     NTOH64(*(uint64_t *)ADDOFF(P,HDR_OFF_TRID))
#define HDR_GET_EXID(P)     NTOH64(*(uint64_t *)ADDOFF(P,HDR_OFF_EXID))

/* Macros to set individual header fields in a raw message. */
#define HDR_SET_TYPE(P,V)   (*(uint16_t *)ADDOFF(P,HDR_OFF_TYPE)  = HTON16(V))
#define HDR_SET_PAYLEN(P,V) (*(uint16_t *)ADDOFF(P,HDR_OFF_PAYLEN)= HTON16(V))
#define HDR_SET_RFU(P,V)    (*(uint32_t *)ADDOFF(P,HDR_OFF_RFU)   = HTON32(V))
#define HDR_SET_TS(P,V)     (*(uint64_t *)ADDOFF(P,HDR_OFF_TS)    = HTON64(V))
#define HDR_SET_SRCID(P,V)  (*(uint64_t *)ADDOFF(P,HDR_OFF_SRCID) = HTON64(V))
#define HDR_SET_DSTID(P,V)  (*(uint64_t *)ADDOFF(P,HDR_OFF_DSTID) = HTON64(V))
#define HDR_SET_TRID(P,V)   (*(uint64_t *)ADDOFF(P,HDR_OFF_TRID)  = HTON64(V))
#define HDR_SET_EXID(P,V)   (*(uint64_t *)ADDOFF(P,HDR_OFF_EXID)  = HTON64(V))


/* Message types. (invalid commented out) */
enum MSG_TYPE {
    MSG_TYPE_REGISTER_IND   = 0x0010,
    MSG_TYPE_REGISTER_REQ   = 0x0011,
    MSG_TYPE_REGISTER_RES   = 0x0012,
    MSG_TYPE_REGISTER_ERR   = 0x001a,
    MSG_TYPE_LOGIN_IND      = 0x0020,
    MSG_TYPE_LOGIN_REQ      = 0x0021,
    MSG_TYPE_LOGIN_RES      = 0x0022,
    MSG_TYPE_LOGIN_ERR      = 0x002a,
    MSG_TYPE_AUTH_IND       = 0x0030,
    MSG_TYPE_AUTH_REQ       = 0x0031,
    MSG_TYPE_AUTH_RES       = 0x0032,
    MSG_TYPE_AUTH_ERR       = 0x003a,
    MSG_TYPE_PEERLIST_IND   = 0x0040,
    MSG_TYPE_PEERLIST_REQ   = 0x0041,
    MSG_TYPE_PEERLIST_RES   = 0x0042,
    MSG_TYPE_PEERLIST_ERR   = 0x004a,
    MSG_TYPE_OFFER_IND      = 0x0110,
    MSG_TYPE_OFFER_REQ      = 0x0111,
    MSG_TYPE_OFFER_RES      = 0x0112,
    MSG_TYPE_OFFER_ERR      = 0x011a,
    MSG_TYPE_GETFILE_IND    = 0x0120,
    MSG_TYPE_GETFILE_REQ    = 0x0121,
    MSG_TYPE_GETFILE_RES    = 0x0122,
    MSG_TYPE_GETFILE_ERR    = 0x012a,
    MSG_TYPE_PING_IND       = 0x0200,
    MSG_TYPE_PING_REQ       = 0x0201,
    MSG_TYPE_PING_RES       = 0x0202,
    MSG_TYPE_PING_ERR       = 0x020a,
};

/* Attributes. */
enum MSG_ATTRIB {
    MSG_ATTR_USERNAME   = 0x0001,
    MSG_ATTR_PUBKEY     = 0x0002,
    MSG_ATTR_CHALLENGE  = 0x0003,
    MSG_ATTR_DIGEST     = 0x0004,
    MSG_ATTR_SIGNATURE  = 0x0005,
    MSG_ATTR_TTL        = 0x0008,
    MSG_ATTR_PEERID     = 0x0010,
    MSG_ATTR_PEERNAME   = 0x0011,
    MSG_ATTR_OFFERID    = 0x0021,
    MSG_ATTR_FILENAME   = 0x0022,
    MSG_ATTR_SIZE       = 0x0023,
    MSG_ATTR_MD5        = 0x0024,
    MSG_ATTR_OFFSET     = 0x0025,
    MSG_ATTR_DATA       = 0x0026,
    MSG_ATTR_OK         = 0x0041,
    MSG_ATTR_ERROR      = 0x0042,
    MSG_ATTR_NOTICE     = 0x0043,
};


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
    size_t bsize;
    size_t boff;
    uint8_t *b; /* Keep b the last member to preserve alignment! */
};


extern mbuf_t *mbuf_new( void );
extern void mbuf_free( mbuf_t **p );
extern mbuf_t *mbuf_resize( mbuf_t **pp, size_t size );
extern mbuf_t *mbuf_grow( mbuf_t **pp, size_t amount );
extern int mbuf_addattrib( mbuf_t **pp, enum MSG_ATTRIB attrib, size_t length, ... );


#ifdef DEBUG
    extern void mhdr_dump( mbuf_t *m );
#else
    #define mhdr_dump(...)
#endif

#endif /* ndef _H_INCLUDED */

/* EOF */
