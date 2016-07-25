/*
 * bendian.h
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


#ifndef BENDIAN_H_INCLUDED
#define BENDIAN_H_INCLUDED

/* Network byte order <--> host byte order conversion. */

/* Sigh. There's a POSIX standard for everything except the most useful
   things. The following is a cheap attempt at making the code at least
   somewhat portable.
*/
#if 0
    /* Use non-standard C library extensions.
       Reportedly breaks on Android, which defines __linux__
       but provides the OpenBSD API. Go figure.
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
    #define NTOH16(V)       be16toh((uint16_t)(V))
    #define NTOH32(V)       be32toh((uint32_t)(V))
    #define NTOH64(V)       be64toh((uint64_t)(V))
    #define HTON16(V)       htobe16((uint16_t)(V))
    #define HTON32(V)       htobe32((uint32_t)(V))
    #define HTON64(V)       htobe64((uint64_t)(V))

#else
    /* This version does not require __DEFAULT_SOURCE defined. */
    #if defined(__linux__)
        #include <endian.h>
    #elif defined(__FreeBSD__) || defined(__NetBSD__)
        #include <sys/endian.h>
    #elif defined(__OpenBSD__)
        #include <sys/types.h>
    #elif defined(__APPLE__)
        # include <libkern/OSByteOrder.h>
        # define __BYTE_ORDER BYTE_ORDER
        # define __BIG_ENDIAN BIG_ENDIAN
        # define __LITTLE_ENDIAN LITTLE_ENDIAN
    #endif
    #if __BYTE_ORDER == __BIG_ENDIAN
        #define NTOH16(V)   (V)
        #define NTOH32(V)   (V)
        #define NTOH64(V)   (V)
        #define HTON16(V)   (V)
        #define HTON32(V)   (V)
        #define HTON64(V)   (V)
    #elif __BYTE_ORDER == __LITTLE_ENDIAN
        #define SWAP2(V) \
         ( (((V) >>  8) & 0x00FF) | (((V) << 8) & 0xFF00) )
        #define SWAP4(V) \
         ( (((V) >> 24) & 0x000000FF) | (((V) >>  8) & 0x0000FF00) | \
           (((V) <<  8) & 0x00FF0000) | (((V) << 24) & 0xFF000000) )
        #define SWAP8(V) \
         ( (((V) >> 56) & 0x00000000000000FF) | (((V) >> 40) & 0x000000000000FF00) | \
           (((V) >> 24) & 0x0000000000FF0000) | (((V) >>  8) & 0x00000000FF000000) | \
           (((V) <<  8) & 0x000000FF00000000) | (((V) << 24) & 0x0000FF0000000000) | \
           (((V) << 40) & 0x00FF000000000000) | (((V) << 56) & 0xFF00000000000000) )
        #define NTOH16(V)   SWAP2(V)
        #define NTOH32(V)   SWAP4(V)
        #define NTOH64(V)   SWAP8(V)
        #define HTON16(V)   SWAP2(V)
        #define HTON32(V)   SWAP4(V)
        #define HTON64(V)   SWAP8(V)
    #else
        #error "Oh no, not these endians again! Go trade your PDP for a real computer!"
    #endif

#endif


#endif /* ndef _H_INCLUDED */

/* EOF */
