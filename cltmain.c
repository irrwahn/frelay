/*
 * cltmain.c
 *
 * Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
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


#define _POSIX_C_SOURCE 201112L

#include <ctype.h>
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

#include "message.h"
#include "cltcfg.h"
#include "util.h"

#include <prng.h>


enum CLT_STATE {
    CLT_INVALID = 0,
    CLT_PRE_LOGIN,
    CLT_LOGIN_OK,
    CLT_AUTH_OK
};

/* Configuration singleton */
static struct {
    struct timeval select_timeout;
    int msg_timeout;
    int conn_timeout;
    const char *username;
    enum CLT_STATE st;
} cfg = {
    /* .select_timeout = */
    { .tv_sec=SEL_TIMEOUT_MS/1000, .tv_usec=SEL_TIMEOUT_MS%1000*1000 },
    /* .msg_timeout = */
    MSG_TIMEOUT_S,
    /* .conn_timeout = */
    CONN_TIMEOUT_S,
    /* .username = */
    "user",
    /* .st = */
    CLT_INVALID,
};

#if 0
/* Transaction structure type */
typedef
    struct TRACT_T_STRUCT
    tract_t;

struct TRACT_T_STRUCT {
    uint64_t id;                /* remote client id */
    time_t act;                 /* time of last activity (s since epoch) */
    mbuf_t *pending;            /* list of pending exchanges */
};
#endif

/**********************************************
 * INITIALIZATION
 *
 */

static int eval_cmdline( int argc, char *argv[] )
{
    return 0;
    (void)argc; (void)argv;
}


/**********************************************
 * SERVER CONNECTION
 *
 */

static int connect_srv( const char *host, const char *service )
{
    int res = 0;
    int fd = -1;
    struct addrinfo hints, *info, *ai;

    /* Create socket, set SO_REUSEADDR, and connect. */
    memset( &hints, 0, sizeof hints );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    res = getaddrinfo( host, service, &hints, &info );
    if ( 0 != res )
    {
        XLOG( LOG_WARNING, "getaddrinfo(%s,%s) failed: %s\n", host, service,
                (EAI_SYSTEM!=res)?gai_strerror(res):strerror(errno) );
        return -1;
    }

    DLOG( "Connecting...\n" );
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
        else if ( 0 != connect( fd, ai->ai_addr, ai->ai_addrlen ) )
        {
            switch ( errno )
            {
            // (hopefully) transient errors
            case ETIMEDOUT:
            case EADDRINUSE:
            case EADDRNOTAVAIL:
            case EAGAIN:
            case EALREADY:
            case ECONNREFUSED:
            case EINPROGRESS:
            case EINTR:
            case ENETUNREACH:
                XLOG( LOG_WARNING, "connect() failed: %m. Retry later.\n" );
                break;
            // (definitely) fatal errors
            case EACCES:
            case EPERM:
            case EAFNOSUPPORT:
            case EBADF:
            case EFAULT:
            case EISCONN:
            case ENOTSOCK:
            case EPROTOTYPE:
            default:
                XLOG( LOG_ERR, "connect() failed miserably: %m. PEBKAC?\n" );
                ai = ai->ai_next;
                break;
            }
            close( fd );
        }
        else
        {   /* all went well */
            break;
        }
    }
    freeaddrinfo( info );
    if ( NULL == ai )
    {
        XLOG( LOG_ERR, "Unable to connect.\n" );
        return -1;
    }
    DLOG( "Connected to [%s:%s].\n", host, service );
    return fd;
}

static int disconnect_srv( int *srvfd )
{
    close( *srvfd );
    *srvfd = -1;
    return 0;
}

/**********************************************
 * MESSAGE PROCESSING
 *
 */

static time_t act = 0;
static int maxfd = -1;
static int srvfd = -1;
static fd_set rfds, wfds;
static mbuf_t *rbuf = NULL;
static mbuf_t *qhead = NULL, *qtail = NULL; /* send queue pointers */

static int enqueue_msg( mbuf_t *m )
{
    DLOG( "%p\n", m );
    m->boff = 0;
    m->next = NULL;
    if ( NULL != qtail )
        qtail->next = m;
    qtail = m;
    if ( NULL == qhead )
        qhead = m;
    return 0;
}

static int dequeue_msg( void )
{
    mbuf_t *next;

    DLOG( "%p\n", qhead );
    mbuf_dump( qhead );

    next = qhead->next;
    if ( qtail == qhead )
        qtail = NULL;
    mbuf_free( &qhead ); // TODO: hook in pending transactions list instead
    qhead = next;
    return 0;
}

/**********************************************
 * I/O HANDLING AND MAIN()
 *
 */

static int process_stdin( void )
{
#define MAX_ARG     10
#define MAX_CMDLINE 4000
    int r;
    int a;
    char *arg[MAX_ARG], *cp;
    static char line[MAX_CMDLINE];

    r = read( STDIN_FILENO, line, sizeof line - 1 );
    if ( 0 > r )
    {
        if ( EAGAIN != errno && EWOULDBLOCK != errno && EINTR != errno )
            XLOG( LOG_ERR, "read() failed miserably: %m.\n" );
        else
            XLOG( LOG_ERR, "read() failed: %m.\n" );
        return -1;
    }
    else if ( 0 == r )
    {
        /* EOF on stdin. */
        return 0;
    }
    line[r] = '\0';

    /* Ye lousy tokeniz0r. */
    for ( a = 0, cp = line; a < MAX_ARG && *cp; ++a )
    {
        arg[a] = cp;
        while ( *cp && !isspace( (unsigned char)*cp ) )
            ++cp;
        while ( isspace( (unsigned char)*cp ) )
            *cp++ = '\0';
    }

    /* Ye shoddy command pars0r. */
    if ( a > 0 && 0 == strcmp( "connect", arg[0] ) )
    {
        mbuf_t *mp = NULL;
        close( srvfd );
        srvfd = connect_srv( a > 1 ? arg[1] : "localhost", a > 2 ? arg[2] : DEF_PORT );
        if ( 0 > srvfd )
            return -1;
        cfg.st = CLT_PRE_LOGIN;
        mbuf_compose( &mp, MSG_TYPE_LOGIN_REQ, 0, 0, random(), 1 );
        mbuf_addattrib( &mp, MSG_ATTR_USERNAME, strlen( cfg.username ) + 1, cfg.username );
        enqueue_msg( mp );
    }
    else if ( a > 0 && 0 == strcmp( "disconnect", arg[0] ) )
    {
        DLOG( "Closing connection.\n" );
        disconnect_srv( &srvfd );
    }
    else if ( a > 0 && 0 == strcmp( "exit", arg[0] ) )
    {
        r = 0;
    }
    else
    {
        printf( "unknowmn command sequence: " );
        for ( int i = 0; i < a; ++i )
            printf( "%s ", arg[i] );
        puts( "" );
    }
    return r;
}

static int process_srvmsg( mbuf_t **pp )
{
    DLOG( "dump:\n" );
    mbuf_dump( *pp );

    uint16_t mtype = HDR_GET_TYPE( *pp );
    uint64_t srcid = HDR_GET_SRCID( *pp );
    uint64_t trid = HDR_GET_TRID( *pp );
    uint64_t exid = HDR_GET_EXID( *pp );
    enum MSG_ATTRIB at;
    size_t al;
    void *av;

    mbuf_resetgetattrib( *pp );
    if ( CLT_AUTH_OK == cfg.st )
    {
        DLOG( "TODO: normal message processing.\n" );
        mbuf_free( pp );
    }
    else if ( CLT_PRE_LOGIN == cfg.st && 0ULL == srcid
        && MSG_TYPE_LOGIN_RES == mtype
        && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
        && MSG_ATTR_CHALLENGE == at )
    {
        DLOG( "Process LOGIN response.\n" );
        cfg.st = CLT_LOGIN_OK;
        void *avcopy = memdup( av, al );
        mbuf_compose( pp, MSG_TYPE_AUTH_REQ, 0, 0, trid, ++exid );
        // TODO: hashing, crypt, whatever
        mbuf_addattrib( pp, MSG_ATTR_DIGEST, al, avcopy );
        enqueue_msg( *pp );
        free( avcopy );
        *pp = NULL;
    }
    else if ( CLT_LOGIN_OK == cfg.st && 0ULL == srcid
        && MSG_TYPE_AUTH_RES == mtype
        && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
        && MSG_ATTR_OK == at )
    {
        DLOG( "Process AUTH response.\n" );
        cfg.st = CLT_AUTH_OK;
        if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_NOTICE == at )
        {
            DLOG( "\n--- Server MotD ---\n%s\n-------------------\n", (char *)av );
        }
        mbuf_free( pp );
    }
    else
    {
        XLOG( LOG_WARNING, "Message not handled.\n" );
        mbuf_free( pp );
    }
    return 0;
}

static int handle_srvio( int nset )
{
    time_t now = time( NULL );

    /* Read from server. */
    if ( FD_ISSET( srvfd, &rfds ) )
    {
        --nset;
        /* Prepare receive buffer. */
        if ( NULL == rbuf )
            mbuf_new( &rbuf );
        act = now;

        if ( rbuf->boff < rbuf->bsize )
        {   /* Message buffer not yet filled. */
            int r;
            errno = 0;
            r = read( srvfd, rbuf->b + rbuf->boff, rbuf->bsize - rbuf->boff );
            if ( 0 > r )
            {
                if ( EAGAIN != errno && EWOULDBLOCK != errno && EINTR != errno )
                {
                    XLOG( LOG_ERR, "read() failed: %m.\n" );
                    disconnect_srv( &srvfd );
                    goto DONE;
                }
                goto SKIP_TO_WRITE;
            }
            if ( 0 == r )
            {
                DLOG( "Remote host closed connection.\n" );
                disconnect_srv( &srvfd );
                goto DONE;
            }
            DLOG( "%d bytes received.\n", r );
            rbuf->boff += r;
        }
        if ( rbuf->boff == rbuf->bsize )
        {   /* Buffer filled. */
            if ( rbuf->boff == MSG_HDR_SIZE )
            {   /* Only received header yet. */
                uint16_t paylen = HDR_GET_PAYLEN( rbuf );
                if ( paylen > 0 )
                {   /* Prepare for receiving payload next. */
                    DLOG( "Grow message buffer by %lu.\n", paylen );
                    mbuf_resize( &rbuf, paylen );
                }
                DLOG( "Expecting %"PRIu16" bytes of payload data.\n", paylen );
            }
            if ( rbuf->boff == rbuf->bsize )
            {   /* Payload data complete. */
                process_srvmsg( &rbuf );
            }
        }
    }
SKIP_TO_WRITE:
    /* Write to server. */
    if ( 0 < nset && FD_ISSET( srvfd, &wfds ) )
    {
        --nset;
        act = now;
        if ( qhead->boff < qhead->bsize )
        {   /* Message buffer not yet fully sent. */
            int w;
            errno = 0;
            w = write( srvfd, qhead->b + qhead->boff, qhead->bsize - qhead->boff );
            if ( 0 > w )
            {
                if ( EAGAIN != errno && EWOULDBLOCK != errno && EINTR != errno )
                {
                    XLOG( LOG_ERR, "write() failed: %m.\n" );
                    disconnect_srv( &srvfd );
                }
                goto DONE;
            }
            if ( 0 == w )
            {
                DLOG( "WTF, write() returned 0: %m.\n" );
                disconnect_srv( &srvfd );
                goto DONE;
            }
            DLOG( "%d bytes sent to server.\n", w );
            qhead->boff += w;
        }
        if ( qhead->boff == qhead->bsize )
        {   /* Message sent, remove from queue. */
            dequeue_msg();
        }
    }
DONE:
    return nset;
}

int main( int argc, char *argv[] )
{
    /* Initialization. */
    XLOG_INIT( argv[0] );
    srandom( time( NULL ) );
    eval_cmdline( argc, argv );
    signal( SIGPIPE, SIG_IGN );     /* Ceci n'est pas une pipe. */
    /* TODO: ignore or handle other signals? */

    DLOG( "Entering main loop.\n" );
    cfg.st = CLT_INVALID;
    while ( 1 )
    {
        static time_t last_upkeep = 0;
        time_t now = time( NULL );
        int nset;
        struct timeval to;

        if ( now - last_upkeep > cfg.select_timeout.tv_sec )
        {   /* Avoid doing upkeep continuously under load. */
            last_upkeep = now;
            /* TODO: send ping to server!? */
        }
        FD_ZERO( &rfds );
        FD_ZERO( &wfds );
        FD_SET( STDIN_FILENO, &rfds );
        if ( 0 <= srvfd )
        {
            FD_SET( srvfd, &rfds );
            if ( NULL != qhead )
                FD_SET( srvfd, &wfds );
            maxfd = srvfd;
        }
        else
            maxfd = STDIN_FILENO;
        to = cfg.select_timeout;
        nset = select( maxfd + 1, &rfds, &wfds, NULL, &to );
        if ( 0 < nset )
        {
            /* DLOG( "%d fds ready.\n", nset ); */
            nset = handle_srvio( nset );
            if ( 0 < nset && FD_ISSET( STDIN_FILENO, &rfds ) )
            {
                --nset;
                /* EOF on stdin terminates server. */
                if ( 0 ==  process_stdin() )
                    break;
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
    XLOG( LOG_INFO, "Terminating.\n" );
    exit( EXIT_SUCCESS );
}

/* EOF */
