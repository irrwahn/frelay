/*
 * srvmain.c
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


#define _POSIX_C_SOURCE 201112L

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>


#ifndef FD_COPY
    /* According to POSIX fd_set is a structure type! */
    #define FD_COPY(dst,src) (*(dst)=*(src))
#endif


#include "srvcfg.h"
#include "message.h"
#include "util.h"


enum CLT_STATE {
    CLT_INVALID = 0,
    CLT_CONNECTED,
    CLT_LOGIN_OK,
    CLT_AUTH_OK
};

/* Client structure type */
typedef
    struct CLIENT_T_STRUCT
    client_t;

struct CLIENT_T_STRUCT {
    int fd;                     /* client socket file descriptor  */
    socklen_t addrlen;          /* client remote address length */
    struct sockaddr_in addr;    /* client remote address */
    uint64_t id;                /* client id */
    enum CLT_STATE st;          /* client state */
    time_t act;                 /* time of last activity (s since epoch) */
    mbuf_t *rbuf;               /* receive buffer */
    mbuf_t *qhead, *qtail;      /* send buffer queue pointers */
};


/* Configuration singleton */
static struct {
    const char *interface;
    const char *listenport;
    struct timeval select_timeout;
    int msg_timeout;
    int conn_timeout;
    int max_clients;
} cfg = {
    /* .interface = */
    NULL,
    /* .listenport = */
    DEF_PORT,
    /* .select_timeout = */
    { .tv_sec=SEL_TIMEOUT_MS/1000, .tv_usec=SEL_TIMEOUT_MS%1000*1000 },
    /* msg_timeout = */
    MSG_TIMEOUT_S,
    /* conn_timeout = */
    CONN_TIMEOUT_S,
    /* .max_clients = */
    MAX_CLIENTS,
};

static int eval_cmdline( int argc, char *argv[] )
{
    XLOG_INIT( argv[0] );

    DLOG( "MSG_HDR_SIZE = %d\n", MSG_HDR_SIZE );
    DLOG( "MSG_MAX_PAY_SIZE = %d\n", MSG_MAX_PAY_SIZE );
    DLOG( "MSG_MAX_SIZE = %d\n", MSG_MAX_SIZE );

    return 0;
    (void)argc;
}

static int init_server( client_t **clients )
{
    int res = 0;
    int fd = -1;
    struct addrinfo hints, *info, *ai;

    /* Initialize clients array. */
    if( FD_SETSIZE < cfg.max_clients )
    {
        XLOG( LOG_WARNING,
            "Maximum number of clients trimmed down to FD_SETZIZE (%d).\n",
            FD_SETSIZE );
        cfg.max_clients = FD_SETSIZE;
    }
    *clients = malloc( cfg.max_clients * sizeof **clients );
    die_if( NULL == *clients, "malloc() failed: %m.\n" );
    memset( *clients, 0, cfg.max_clients * sizeof **clients );
    for ( int i = 0; i < cfg.max_clients; ++i )
        (*clients)[i].fd = -1;

    /* Create socket, set SO_REUSEADDR, bind and listen. */
    memset( &hints, 0, sizeof hints );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    res = getaddrinfo( cfg.interface, cfg.listenport, &hints, &info );
    die_if( 0 != res,
            "getaddrinfo(%s,%s) failed: %s\n", cfg.interface, cfg.listenport,
            (EAI_SYSTEM!=res)?gai_strerror(res):strerror(errno) );
    for ( ai = info; NULL != ai; ai = ai->ai_next )
    {
        int set = 1;
        if ( 0 > ( fd = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol ) ) )
        {
            XLOG( LOG_WARNING, "socket() failed: %m.\n" );
        }
        else if ( 0 != setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof set ) )
        {
            XLOG( LOG_WARNING, "setsockopt() failed: %m.\n" );
            close( fd );
        }
        else if ( 0 != bind( fd, ai->ai_addr, ai->ai_addrlen ) )
        {
            XLOG( LOG_WARNING, "bind() failed: %m.\n" );
            close( fd );
        }
        else
        {   /* all went well */
            struct sockaddr_in *ain = (struct sockaddr_in *)ai->ai_addr;
            XLOG( LOG_INFO, "Server bound to interface %s.\n",
                    inet_ntoa( ain->sin_addr ) );
            break;
        }
    }
    freeaddrinfo( info );
    die_if( NULL == ai, "Unable to bind to any interface.\n" );
    res = listen( fd, 1 );
    die_if( 0 != res, "listen() failed: %m.\n" );
    XLOG( LOG_INFO, "Server listening on port %s.\n", cfg.listenport );
    return fd;
}

static int close_client( client_t *cp, fd_set *m_rfds, fd_set *m_wfds )
{
    DLOG( "Closing connection to [%s:%hu].\n",
        inet_ntoa( cp->addr.sin_addr ), cp->addr.sin_port );
    FD_CLR( cp->fd, m_rfds );
    FD_CLR( cp->fd, m_wfds );
    close( cp->fd );
    cp->fd = -1;
    cp->id = 0ULL;
    cp->st = CLT_INVALID;
    if ( NULL != cp->rbuf )
        mbuf_free( &cp->rbuf );
    for ( mbuf_t *qp = cp->qhead, *next; NULL != qp; qp = next )
    {
        next = qp->next;
        mbuf_free( &qp );
    }
    cp->qhead = NULL;
    cp->qtail = NULL;
    return 0;
}

static int accept_client( client_t *clients, int lfd, int *pmaxfd, fd_set *m_rfds )
{
    int i, fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof addr;

    /* Preliminarily accept the connection. */
    fd = accept( lfd, (struct sockaddr *)&addr, &addrlen );
    return_if( -1 == fd, -1, "accept() failed: %m.\n" );
    DLOG( "Accepted connection %d from [%s:%hu].\n",
            fd, inet_ntoa( addr.sin_addr ), addr.sin_port );
    if ( FD_SETSIZE <= fd )
    {
        XLOG( LOG_ERR, "FD_SETSIZE exceeded, dropping connection %d.\n", fd );
        close( fd );
        return -1;
    }
    if ( 0 != set_nonblocking( fd ) )
    {
        XLOG( LOG_ERR, "set_nonblocking() failed, dropping connection %d.\n", fd );
        close( fd );
        return -1;
    }
    /* Find an unused client slot. */
    for ( i = 0; i < cfg.max_clients && 0 <= clients[i].fd; ++i )
        continue;
    if ( i == cfg.max_clients )
    {
        XLOG( LOG_ERR, "No client slot available, dropping connection %d.\n", fd );
        close( fd );
        return -1;
    }
    /* Ultimately adopt connection. */
    clients[i].fd = fd;
    clients[i].addrlen = addrlen;
    clients[i].addr = addr;
    clients[i].id = 0ULL;  /* Set upon successful authentication! */
    XLOG( LOG_WARNING, "!!!!!!!!!!!!!!!!!! Remove AUTH_OK override in production!!!!!!!!!!!!!!\n" );
    clients[i].st = CLT_AUTH_OK; // !!! CLT_CONNECTED;
    clients[i].act = time( NULL );
    clients[i].rbuf = NULL;
    clients[i].qhead = NULL;
    clients[i].qtail = NULL;
    FD_SET( fd, m_rfds );
    if ( fd > *pmaxfd )
        *pmaxfd = fd;
    return i;
}

static int resync_client( client_t *cp, time_t now )
{
    /* Detect message timeouts (gaps). */
    if ( NULL != cp->rbuf && now - cp->act > cfg.msg_timeout )
    {
        DLOG( "Intra-message gap detected, resynching.\n" );
        mbuf_free( &cp->rbuf );
        return 1;
    }
    return 0;
}

static int enq_message( client_t *cp, mbuf_t *m )
{
    DLOG( "%p\n", m );
    m->next = NULL;
    if ( NULL != cp->qtail )
        cp->qtail->next = m;
    cp->qtail = m;
    if ( NULL == cp->qhead )
        cp->qhead = m;
    return 0;
}

static int deq_message( client_t *cp )
{
    mbuf_t *next;

    DLOG( "%p\n", cp->qhead );
    next = cp->qhead->next;
    if ( cp->qtail == cp->qhead )
        cp->qtail = NULL;
    mbuf_free( &cp->qhead );
    cp->qhead = next;
    return 0;
}

static int upkeep( client_t *c, int *maxfd, fd_set *m_rfds, fd_set *m_wfds )
{
    int i, j;
    time_t now = time( NULL );
    int x = 0, base = 0, top = 0;

    for ( i = j = 0; i < cfg.max_clients; ++i )
    {
        if ( 0 <= c[i].fd )
        {
            if ( now - c[i].act > cfg.conn_timeout )
            {   /* Dispose of timed out clients. */
                close_client( &c[i], m_rfds, m_wfds );
                ++x;
            }
            else
                resync_client( &c[i], now );
        }
        if ( 0 <= c[i].fd )
        {   /* Client still valid: adjust maxfd and budge up. */
            if ( *maxfd < c[i].fd )
                *maxfd = c[i].fd;
            top = i;
            base = j;
            if ( j != i )
            {
                c[j] = c[i];
                c[i].fd = -1;
            }
            ++j;
        }
    }
    if ( x )
        DLOG( "Closed %d expired connection(s).\n", x );
    if ( top - base )
        DLOG( "Compacted client list by %d.\n", top - base );
    return 0;
}

static int process_server_msg( client_t *c, int i_src, fd_set *m_wfds )
{
    switch ( c[i_src].rbuf->hdr.type )
    {
    case MSG_TYPE_REGISTER_REQ:
    case MSG_TYPE_LOGIN_REQ:
    case MSG_TYPE_AUTH_REQ:  
    case MSG_TYPE_PEERLIST_REQ:
    case MSG_TYPE_PING_IND:
    case MSG_TYPE_PING_REQ:           
        DLOG( "TODO: process message directed at server.\n" );
            (void)i_src; (void)m_wfds; mbuf_free( &c[i_src].rbuf );//REMOVE THIS LATER!
        return 0;
        break;
    // Anything else is nonsense
    default: 
        break;
    }
    DLOG( "TODO: message type 0x%04x not understood by server.\n", c[i_src].rbuf->hdr.type );
    return -1;
}

static int process_broadcast_msg( client_t *c, int i_src, fd_set *m_wfds )
{
    DLOG( "TODO: implement processing of broadcast messages.\n" );
    (void)c;
    (void)i_src;
    (void)m_wfds;
    return -1;
}

static int process_forward_msg( client_t *c, int i_src, fd_set *m_wfds )
{
    int i_dst;
    
    for ( i_dst = 0; i_dst < cfg.max_clients; ++i_dst )
    {
        if ( 0 <= c[i_dst].fd 
            && CLT_AUTH_OK == c[i_dst].st
            && c[i_src].rbuf->hdr.dstid == c[i_dst].id )
            break;
    }
    if ( i_dst == cfg.max_clients )
    {
        DLOG( "TODO: process messages directed at invalid or unknown client.\n" );
        return -1;
    }
    switch ( c[i_src].rbuf->hdr.type )
    {
    case MSG_TYPE_OFFER_IND:
    case MSG_TYPE_OFFER_REQ:  
    case MSG_TYPE_OFFER_RES:  
    case MSG_TYPE_OFFER_ERR:  
    case MSG_TYPE_GETFILE_REQ:
    case MSG_TYPE_GETFILE_RES:
    case MSG_TYPE_GETFILE_ERR:
    case MSG_TYPE_PING_IND:
    case MSG_TYPE_PING_REQ:   
    case MSG_TYPE_PING_RES:   
    case MSG_TYPE_PING_ERR:   
        DLOG( "Add message to c[%d] send queues and add client to m_wfds.\n", i_dst );
        c[i_src].rbuf->boff = 0;
        enq_message( &c[i_dst], c[i_src].rbuf );
        FD_SET( c[i_dst].fd, m_wfds );
        return 0;
        break;
    // Anything else is nonsense
    default: 
        break;
    }
    DLOG( "TODO: message type not suited for destination.\n" );
    return -1;
}

static int process_msg( client_t *c, int i_src, fd_set *m_wfds )
{
    int r;
    
    DLOG("\n");
    mhdr_dump( c[i_src].rbuf );

    DLOG( "TODO: fill in *real* source ID.\n" );
    c[i_src].id = c[i_src].rbuf->hdr.srcid;
    
    if ( 0ULL == c[i_src].rbuf->hdr.dstid )
        r = process_server_msg( c, i_src, m_wfds );
    else if ( -1ULL == c[i_src].rbuf->hdr.dstid )
        r = process_broadcast_msg( c, i_src, m_wfds );
    else
        r = process_forward_msg( c, i_src, m_wfds );
    if ( 0 != r )
        mbuf_free( &c[i_src].rbuf );
    return r;
}

static int handle_clients( client_t *c, int nset,
            fd_set *rfds, fd_set *wfds, fd_set *m_rfds, fd_set *m_wfds )
{
    int i;
    time_t now = time( NULL );

    for ( i = 0; i < cfg.max_clients && 0 < nset; ++i )
    {
        if ( 0 > c[i].fd )
            continue;

        /* Handle fds ready for reading. */
        if ( FD_ISSET( c[i].fd, rfds ) )
        {
            --nset;
            /* Detect message timeouts. */
            resync_client( &c[i], now );
            /* Prepare receive buffer. */
            if ( NULL == c[i].rbuf )
                c[i].rbuf = mbuf_new();
            c[i].act = now;

            if ( c[i].rbuf->boff < c[i].rbuf->bsize )
            {   /* Message buffer not yet filled. */
                int r;
                errno = 0;
                r = read( c[i].fd, c[i].rbuf->b + c[i].rbuf->boff,
                            c[i].rbuf->bsize - c[i].rbuf->boff );
                if ( 0 > r )
                {
                    if ( EAGAIN != errno
                        && EWOULDBLOCK != errno && EINTR != errno )
                    {
                        XLOG( LOG_ERR, "read() failed: %m.\n" );
                        close_client( &c[i], m_rfds, m_wfds );
                        continue;
                    }
                    goto SKIP_TO_WRITE;
                }
                if ( 0 == r )
                {
                    DLOG( "Client closed connection.\n" );
                    close_client( &c[i], m_rfds, m_wfds );
                    continue;
                }
                DLOG( "%d bytes received from c[%d]\n", r, i );
                c[i].rbuf->boff += r;
            }
            if ( c[i].rbuf->boff == c[i].rbuf->bsize )
            {   /* Buffer filled. */
                if ( c[i].rbuf->boff == MSG_HDR_SIZE )
                {   /* Only received header yet. */
                    mhdr_decode( c[i].rbuf );
                    if ( c[i].rbuf->hdr.paylen > 0 )
                    {   /* Prepare for receiving payload next. */
                        DLOG( "Grow message buffer by %lu.\n", c[i].rbuf->hdr.paylen );
                        mbuf_resize( &c[i].rbuf, c[i].rbuf->hdr.paylen );
                    }
                    DLOG( "Expecting %"PRIu16" bytes of payload data.\n", c[i].rbuf->hdr.paylen );
                }
                if ( c[i].rbuf->boff == c[i].rbuf->bsize )
                {   /* Payload data complete. */
                    process_msg( c, i, m_wfds );                    
                    c[i].rbuf = NULL;
                }
            }
        }
    SKIP_TO_WRITE:
        /* Handle fds ready for writing. */
        if ( FD_ISSET( c[i].fd, wfds ) )
        {
            --nset;
            c[i].act = time( NULL );
            if ( c[i].qhead->boff < c[i].qhead->bsize )
            {   /* Message buffer not yet fully sent. */
                int w;
                errno = 0;
                w = write( c[i].fd, c[i].qhead->b + c[i].qhead->boff,
                            c[i].qhead->bsize - c[i].qhead->boff );
                if ( 0 > w )
                {
                    if ( EAGAIN != errno
                        && EWOULDBLOCK != errno && EINTR != errno )
                    {
                        XLOG( LOG_ERR, "write() failed: %m.\n" );
                        close_client( &c[i], m_rfds, m_wfds );
                    }
                    continue;
                }
                if ( 0 == w )
                {
                    DLOG( "WTF, write() returned 0: %m.\n" );
                    close_client( &c[i], m_rfds, m_wfds );
                    continue;
                }
                DLOG( "%d bytes sent to c[%d]\n", w, i );
                c[i].qhead->boff += w;
            }
            if ( c[i].qhead->boff == c[i].qhead->bsize )
            {   /* Message sent, remove from queue. */
                deq_message( &c[i] );
                if ( NULL == c[i].qhead )
                    FD_CLR( c[i].fd, m_wfds );
            }
        }
    }
    return nset;
}

int main( int argc, char *argv[] )
{
    int maxfd = -1;
    int listenfd;
    fd_set m_rfds, m_wfds;
    client_t *clients;

    /* Initialization. */
    eval_cmdline( argc, argv );
    signal( SIGPIPE, SIG_IGN );     /* Ceci n'est pas une pipe. */
    /* TODO: ignore or handle other signals? */

    /* Bring up the server. */
    listenfd = init_server( &clients );
    FD_ZERO( &m_rfds );
    FD_ZERO( &m_wfds );
    FD_SET( STDIN_FILENO, &m_rfds );
    FD_SET( listenfd, &m_rfds );
    DLOG( "Entering main loop.\n" );
    while ( 1 )
    {
        static time_t last_upkeep = 0;
        time_t now = time( NULL );
        int nset;
        fd_set rfds, wfds;
        struct timeval to;

        if ( now - last_upkeep > cfg.select_timeout.tv_sec )
        {   /* Avoid doing upkeep continuously under load. */
            last_upkeep = now;
            maxfd = listenfd;
            upkeep( clients, &maxfd, &m_rfds, &m_wfds );
        }
        FD_COPY( &rfds, &m_rfds );
        FD_COPY( &wfds, &m_wfds );
        to = cfg.select_timeout;
        nset = select( maxfd + 1, &rfds, &wfds, NULL, &to );
        if ( 0 < nset )
        {
            /* DLOG( "%d fds ready.\n", nset ); */
            if ( FD_ISSET( listenfd, &rfds ) )
            {
                --nset;
                accept_client( clients, listenfd, &maxfd, &m_rfds );
            }
            if ( 0 < nset && FD_ISSET( STDIN_FILENO, &rfds ) )
            {
                --nset;
#ifdef DEBUG
                /* EOF on stdin terminates server. */
                if ( 0 == drain_fd( STDIN_FILENO ) )
                    break;
#endif
            }
            if ( 0 < nset )
            {
                nset = handle_clients( clients, nset,
                                    &rfds, &wfds, &m_rfds, &m_wfds );
            }
            if ( 0 < nset )
            {
                XLOG( LOG_ERR, "%d file descriptors still set!\n", nset );
            }
        }
        else if ( 0 == nset )
        {
            /* DLOG( "select timed out.\n" ); */
        }
        else if ( EINTR == errno )
        {
            DLOG( "select() was interrupted: %m.\n" );
        }
        else
        {   /* FATAL ERRORS
               Unable to allocate memory for internal tables. */
            die_if( ENOMEM == errno, "select failed: %m.\n" );
            /* An invalid file descriptor was given in one of the sets.
               (Perhaps a file descriptor that was already closed,
               or one on which an error has occurred.) */
            die_if( EBADF == errno,  "select failed: %m.\n" );
            /* nfds is negative or exceeds the RLIMIT_NOFILE resource
               limit (see getrlimit(2)), or the value contained within
               timeout is invalid. */
            die_if( EINVAL == errno, "select failed: %m.\n" );
            /* This should never happen! */
            die_if( 1, "unhandled error in select(): %m (%d).\n", errno );
        }
    }
    /* Never reached during normal (non-debug) operation. */
    XLOG( LOG_INFO, "Terminating.\n" );
    exit( EXIT_SUCCESS );
}

/* EOF */