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

enum transfer_type {
    TTYPE_OFFER,
    TTYPE_DOWNLOAD,
};

/* Offer structure type to keep information aboput offers and downloads. */
typedef
    struct TRANSFER_T_STRUCT
    transfer_t;

struct TRANSFER_T_STRUCT {
    enum transfer_type type;/* offer or download */
    uint64_t rid;           /* remote client id */
    uint64_t oid;           /* offer id */
    char *name;             /* file name */
    char *partname;         /* partfile name */
    uint64_t size;          /* file size */
    uint64_t offset;        /* file offset for downloads only */
    int fd;                 /* file descriptor to offer/download file */
    time_t tstart;          /* creation time */
    time_t tact;            /* time of last activity (s since epoch) */
    transfer_t *next;
};

extern int64_t fsize( const char *filename );
extern int make_filenames( transfer_t *d, const char *s, int no_clobber );

/*
    Format specifiers for transfer_itostr() parameter 'fmt':
        %i:    ID in "type,remote_id,transfer_id" format
        %n:    file name
        %N:    partfile name
        %o:    current offset in bytes
        %O:    human readable offset
        %s:    size in bytes
        %S:    human readable size
        %d:    duration in seconds
        %D:    human readable duration
        %a:    time elapsed since last activity in seconds
        %A:    human readable time elapsed since last activity
        %t:    start time in seconds since epoch
        %T:    start date and time (local) in full ISO 8601 format
*/
extern char *transfer_itostr( char *buf, size_t size, const char *fmt, const transfer_t *t );
extern int transfer_strtoi( const char *s, enum transfer_type *ptype, uint64_t *prid, uint64_t *poid );

extern transfer_t *transfer_new( enum transfer_type type );
extern transfer_t *transfer_match( enum transfer_type type, uint64_t rid, uint64_t oid );
extern void transfer_closeall( void );
extern int transfer_invalidate( transfer_t *p );
extern void transfer_upkeep( time_t timeout );
extern int transfer_list( int (*cb)(transfer_t *) );
extern int transfer_remove( const char *s );


extern transfer_t *offer_new( uint64_t dest, const char *filename );
extern void *offer_read( transfer_t *o, uint64_t off, uint64_t *psz );

extern transfer_t *download_new( void );
extern int download_write( transfer_t *d, void *data, size_t sz );
extern int download_resume( transfer_t *d );


#endif /* ndef _H_INCLUDED */

/* EOF */
