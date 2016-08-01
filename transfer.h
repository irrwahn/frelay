/*
 * transfer.h
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


#ifndef OFFER_H_INCLUDED
#define OFFER_H_INCLUDED


#include <stdio.h>
#include <time.h>


/* Offer structure type to keep information aboput offers and downloads. */
typedef
    struct TRANSFER_T_STRUCT
    transfer_t;

struct TRANSFER_T_STRUCT {
    uint64_t rid;           /* remote client id */
    uint64_t oid;           /* offer id */
    char *name;             /* file name */
    char *partname;         /* partfile name */
    uint64_t size;          /* file size */
    // TODO hash ?
    // TODO ttl ?
    uint64_t offset;        /* file offset for downloads only */
    int fd;                 /* file descriptor to offer/download file */
    time_t act;             /* time of last activity (s since epoch) */
    transfer_t *next;
};


extern transfer_t *offer_new( uint64_t dest, const char *filename );
extern transfer_t *offer_match( uint64_t oid, uint64_t rid );
extern void *offer_read( transfer_t *o, uint64_t off, size_t *psz );

extern transfer_t *download_new( void );
extern transfer_t *download_match( uint64_t oid );
extern int download_write( transfer_t *d, void *data, size_t sz );
extern int download_resume( transfer_t *d );

extern void transfer_closeall( void );
extern void transfer_upkeep( time_t timeout );


#endif /* ndef _H_INCLUDED */

/* EOF */
