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

#include "statcodes.h"
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

/* Macros to construct or inspect message types. */
#define MCLASS_IND       0x0000
#define MCLASS_REQ       0x0001
#define MCLASS_RES       0x0002
#define MCLASS_ERR       0x000a

#define MTYPE_REGISTER   0x0010
#define MTYPE_LOGIN      0x0020
#define MTYPE_AUTH       0x0030
#define MTYPE_LOGOUT     0x0040
#define MTYPE_PEERLIST   0x0050
#define MTYPE_OFFER      0x0110
#define MTYPE_GETFILE    0x0120
#define MTYPE_PING       0x0200

#define MTYPE_GET_CLASS(T)  ((T) & 0x000f)
#define MTYPE_CUT_CLASS(T)  ((T) & 0xfff0)

#define MCLASS_IS_IND(T)     (MCLASS_IND == MTYPE_GET_CLASS(T))
#define MCLASS_IS_REQ(T)     (MCLASS_REQ == MTYPE_GET_CLASS(T))
#define MCLASS_IS_RES(T)     (MCLASS_RES == MTYPE_GET_CLASS(T))
#define MCLASS_IS_ERR(T)     (MCLASS_ERR == MTYPE_GET_CLASS(T))

#define HDR_TYPE_IS_IND(P)  MTYPE_IS_IND(HDR_GET_TYPE(P))
#define HDR_TYPE_IS_REQ(P)  MTYPE_IS_REQ(HDR_GET_TYPE(P))
#define HDR_TYPE_IS_RES(P)  MTYPE_IS_RES(HDR_GET_TYPE(P))
#define HDR_TYPE_IS_ERR(P)  MTYPE_IS_ERR(HDR_GET_TYPE(P))

/* Macros to convert message classes. */
#define MCLASS_TO_RES(T)     (MTYPE_CUT_CLASS(T) | 0x0002)
#define MCLASS_TO_ERR(T)     (MTYPE_CUT_CLASS(T) | 0x000a)

#define HDR_CLASS_TO_RES(P)  HDR_SET_TYPE((P),MCLASS_TO_RES(HDR_GET_TYPE(P)))
#define HDR_CLASS_TO_ERR(P)  HDR_SET_TYPE((P),MCLASS_TO_ERR(HDR_GET_TYPE(P)))


/* Fully qualified message types, unused commented out. */
enum MSG_TYPE {
  //MSG_TYPE_REGISTER_IND  = (MTYPE_REGISTER | MCLASS_IND),   // 0x0010,
    MSG_TYPE_REGISTER_REQ  = (MTYPE_REGISTER | MCLASS_REQ),   // 0x0011,
    MSG_TYPE_REGISTER_RES  = (MTYPE_REGISTER | MCLASS_RES),   // 0x0012,
    MSG_TYPE_REGISTER_ERR  = (MTYPE_REGISTER |MCLASS_ERR),    // 0x001a,
  //MSG_TYPE_LOGIN_IND     = (MTYPE_LOGIN | MCLASS_IND),      // 0x0020,
    MSG_TYPE_LOGIN_REQ     = (MTYPE_LOGIN | MCLASS_REQ),      // 0x0021,
    MSG_TYPE_LOGIN_RES     = (MTYPE_LOGIN | MCLASS_RES),      // 0x0022,
    MSG_TYPE_LOGIN_ERR     = (MTYPE_LOGIN | MCLASS_ERR),      // 0x002a,
  //MSG_TYPE_AUTH_IND      = (MTYPE_AUTH | MCLASS_IND),       // 0x0030,
    MSG_TYPE_AUTH_REQ      = (MTYPE_AUTH | MCLASS_REQ),       // 0x0031,
    MSG_TYPE_AUTH_RES      = (MTYPE_AUTH | MCLASS_RES),       // 0x0032,
    MSG_TYPE_AUTH_ERR      = (MTYPE_AUTH | MCLASS_ERR),       // 0x003a,
  //MSG_TYPE_LOGOUT_IND    = (MTYPE_LOGOUT | MCLASS_IND),     // 0x0040,
    MSG_TYPE_LOGOUT_REQ    = (MTYPE_LOGOUT | MCLASS_REQ),     // 0x0041,
    MSG_TYPE_LOGOUT_RES    = (MTYPE_LOGOUT | MCLASS_RES),     // 0x0042,
    MSG_TYPE_LOGOUT_ERR    = (MTYPE_LOGOUT | MCLASS_ERR),     // 0x004a,
  //MSG_TYPE_PEERLIST_IND  = (MTYPE_PEERLIST | MCLASS_IND),   // 0x0050,
    MSG_TYPE_PEERLIST_REQ  = (MTYPE_PEERLIST | MCLASS_REQ),   // 0x0051,
    MSG_TYPE_PEERLIST_RES  = (MTYPE_PEERLIST | MCLASS_RES),   // 0x0052,
    MSG_TYPE_PEERLIST_ERR  = (MTYPE_PEERLIST | MCLASS_ERR),   // 0x005a,
    MSG_TYPE_OFFER_IND     = (MTYPE_OFFER | MCLASS_IND),      // 0x0110,
    MSG_TYPE_OFFER_REQ     = (MTYPE_OFFER | MCLASS_REQ),      // 0x0111,
    MSG_TYPE_OFFER_RES     = (MTYPE_OFFER | MCLASS_RES),      // 0x0112,
    MSG_TYPE_OFFER_ERR     = (MTYPE_OFFER | MCLASS_ERR),      // 0x011a,
  //MSG_TYPE_GETFILE_IND   = (MTYPE_GETFILE | MCLASS_IND),    // 0x0120,
    MSG_TYPE_GETFILE_REQ   = (MTYPE_GETFILE | MCLASS_REQ),    // 0x0121,
    MSG_TYPE_GETFILE_RES   = (MTYPE_GETFILE | MCLASS_RES),    // 0x0122,
    MSG_TYPE_GETFILE_ERR   = (MTYPE_GETFILE | MCLASS_ERR),    // 0x012a,
    MSG_TYPE_PING_IND      = (MTYPE_PING | MCLASS_IND),       // 0x0200,
    MSG_TYPE_PING_REQ      = (MTYPE_PING | MCLASS_REQ),       // 0x0201,
    MSG_TYPE_PING_RES      = (MTYPE_PING | MCLASS_RES),       // 0x0202,
    MSG_TYPE_PING_ERR      = (MTYPE_PING | MCLASS_ERR),       // 0x020a,
};

/* Attributes. */
enum MSG_ATTRIB {
    MSG_ATTR_INVALID    = 0x0000,
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
    struct MBUF_T_STRUCT
    mbuf_t;

struct MBUF_T_STRUCT {
    mbuf_t *next;
    size_t bsize;
    size_t boff;
    uint8_t *b; /* Keep b the last member to preserve alignment! */
};


extern mbuf_t *mbuf_new( mbuf_t **pp );
extern void mbuf_free( mbuf_t **p );
extern mbuf_t *mbuf_resize( mbuf_t **pp, size_t size );
extern mbuf_t *mbuf_grow( mbuf_t **pp, size_t amount );
extern mbuf_t *mbuf_compose( mbuf_t **pp, enum MSG_TYPE type,
                    uint64_t srcid, uint64_t dstid,
                    uint64_t trid, uint64_t exid );
extern mbuf_t *mbuf_to_response( mbuf_t **pp );
extern mbuf_t *mbuf_to_error_response( mbuf_t **pp, enum SC_ENUM ec );

extern int mbuf_addattrib( mbuf_t **pp, enum MSG_ATTRIB attrib, size_t length, ... );
extern int mbuf_getnextattrib( mbuf_t *p, enum MSG_ATTRIB *ptype, size_t *plen, void **pval );
extern int mbuf_resetgetattrib( mbuf_t *p );

extern const char *mtype2str( int mtype );
extern const char *mclass2str( int mtype );


#ifdef DEBUG
    extern void mbuf_dump( mbuf_t *m );
#else
    #define mbuf_dump(m)  ((void)0)
#endif

#endif /* ndef _H_INCLUDED */

/* EOF */
