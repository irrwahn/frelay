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
#include <stdarg.h>
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
#include <stricmp.h>


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
    const char *pubkey;
    const char *privkey;
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
    /* .pubkey = */
    "userpubkey",
    /* .privkey = */
    "userprivkey",
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

static int disconnect_srv( int *pfd )
{
    if ( 0 > *pfd )
        return -1;
    close( *pfd );
    *pfd = -1;
    return 0;
}

static int connect_srv( int *pfd, const char *host, const char *service )
{
    int res = 0;
    int fd = -1;
    struct addrinfo hints, *info, *ai;

    disconnect_srv( pfd );
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
    XLOG( LOG_INFO, "Connected to [%s:%s].\n", host, service );
    *pfd = fd;
    return 0;
}

/**********************************************
 * MESSAGE PROCESSING
 *
 */

static mbuf_t *rbuf = NULL;
static mbuf_t *qhead = NULL, *qtail = NULL; /* send queue pointers */

static int enqueue_msg( mbuf_t *m )
{
    //DLOG( "\n" ); mbuf_dump( m );
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

    //DLOG( "\n" ); mbuf_dump( qhead );
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

/*
 * Just to have a central function to control console output.
 */
static int printcon( const char *fmt, ... )
{
    int r;
    va_list arglist;
    FILE *ofp = stdout;

    va_start( arglist, fmt );
    r = vfprintf( ofp, fmt, arglist );
    va_end( arglist );
    fflush( ofp );
    return r;
}

static int process_stdin( int *srvfd )
{
#define MAX_ARG     10
#define MAX_CMDLINE 4000
    mbuf_t *mp = NULL;
    int r;
    int a;
    char *cp;
    const char *arg[MAX_ARG];
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
    {   /* EOF on stdin. */
        return 0;
    }
    line[r] = '\0';

    /* Ye lousy tokeniz0r. */
    for ( a = 0, cp = line; *cp && a < MAX_ARG; ++a )
    {
        if ( '"' == *cp )
        {
            arg[a] = ++cp;
            while ( *cp && '"' != *cp )
            {
                if ( '\\' == *cp && '"' == *(cp + 1) )
                    ++cp;
                ++cp;
            }
            if ( '"' == *cp )
                *cp++ = '\0';
        }
        else
        {
            arg[a] = cp;
            while ( *cp && !isspace( (unsigned char)*cp ) )
                ++cp;
        }
        while ( isspace( (unsigned char)*cp ) )
            *cp++ = '\0';
    }
#undef MAX_ARG
#undef MAX_CMDLINE

    /* Ye shoddy command pars0r. */
#define MATCH_CMD(S)   ( 0 == strnicmp((S),arg[0],strlen(arg[0])) )
    if ( '\0' == arg[0][0] )
        return -1;
    if ( MATCH_CMD( "ping" ) )
    {   /* ping [destination [notice]] */
        mbuf_compose( &mp, MSG_TYPE_PING_REQ, 0,
                ( 1 < a ) ? strtoull( arg[1], NULL, 16 ) : 0, random(), 1 );
        if ( 2 < a )
            mbuf_addattrib( &mp, MSG_ATTR_NOTICE, strlen( arg[2] ) + 1, arg[2] );
    }
    else if ( MATCH_CMD( "peerlist" ) )
    {
        mbuf_compose( &mp, MSG_TYPE_PEERLIST_REQ, 0, 0, random(), 1 );
    }
    else if ( MATCH_CMD( "connect" ) )
    {   /* connect [host [port]] */
        if ( 3 > a )
            arg[2] = DEF_PORT;
        if ( 2 > a )
            arg[1] = "localhost";
        connect_srv( srvfd, arg[1], arg[2] );
        if ( 0 > *srvfd )
        {
            printcon( "Connect attempt failed.\n" );
            cfg.st = CLT_INVALID;
            return -1;
        }
        printcon( "Connected.\n" );
        cfg.st = CLT_PRE_LOGIN;
    }
    else if ( MATCH_CMD( "disconnect" ) )
    {
        if ( 0 <= *srvfd )
        {
            DLOG( "Closing connection.\n" );
            disconnect_srv( srvfd );
            printcon( "Disconnected by request.\n" );
        }
        else
            printcon( "Not connected.\n" );
        cfg.st = CLT_INVALID;
    }
    else if ( MATCH_CMD( "register" ) )
    {   /* register [user [pubkey]] */
        DLOG( "Registering.\n" );
        if ( 3 > a )
            arg[2] = cfg.username;
        if ( 2 > a )
            arg[1] = cfg.pubkey;
        mbuf_compose( &mp, MSG_TYPE_REGISTER_REQ, 0, 0, random(), 1 );
        mbuf_addattrib( &mp, MSG_ATTR_USERNAME, strlen( arg[1] ) + 1, arg[1] );
        mbuf_addattrib( &mp, MSG_ATTR_PUBKEY, strlen( arg[2] ) + 1, arg[2] );
    }
    else if ( MATCH_CMD( "login" ) )
    {   /* login [user] */
        DLOG( "Logging in.\n" );
        if ( 2 > a )
            arg[1] = cfg.username;
        mbuf_compose( &mp, MSG_TYPE_LOGIN_REQ, 0, 0, random(), 1 );
        mbuf_addattrib( &mp, MSG_ATTR_USERNAME, strlen( arg[1] ) + 1, arg[1] );
    }
    else if ( MATCH_CMD( "logout" ) )
    {
        DLOG( "Logging out.\n" );
        mbuf_compose( &mp, MSG_TYPE_LOGOUT_REQ, 0, 0, random(), 1 );
    }
    else if ( MATCH_CMD( "exit" ) )
    {
        disconnect_srv( srvfd );
        cfg.st = CLT_INVALID;
        return 0;
    }
    else
    {
        printcon( "Invalid command.\n" );
        DLOG( "unknown command sequence: \n" );
        for ( int i = 0; i < a; ++i )
            DLOG( "> %s\n", arg[i] );
    }
#undef MATCH_CMD    

    if ( NULL != mp )
    {
        if ( 0 > *srvfd )
            printcon( "Not connected.\n" );
        else
            enqueue_msg( mp );
    }
    return r;
}

static int process_srvmsg( mbuf_t **pp )
{
    uint16_t mtype = HDR_GET_TYPE( *pp );
    uint64_t srcid = HDR_GET_SRCID( *pp );
    uint64_t trid = HDR_GET_TRID( *pp );
    uint64_t exid = HDR_GET_EXID( *pp );
    mbuf_t *mp = NULL;
    enum MSG_ATTRIB at;
    size_t al;
    void *av;
    int res = 0;

    DLOG( "Received %s_%s from 0x%016"PRIx64".\n",
            mtype2str( mtype ), mclass2str( mtype ), srcid );
    mbuf_resetgetattrib( *pp );
    switch ( mtype )
    {
    /* Indications and Requests */
    case MSG_TYPE_PING_REQ:
    case MSG_TYPE_PING_IND:
        if ( CLT_AUTH_OK == cfg.st )
        {
            char *s = NULL;
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                s = (char *)av;
            printcon( "Ping from 0x%016"PRIx64"%s%s%s\n",
                        srcid, s?": '":"", s?s:"", s?"'":"" );
            if ( MCLASS_IS_REQ( mtype ) )
            {
                mbuf_compose( &mp, MSG_TYPE_PING_RES, 0, srcid, trid, ++exid );
                mbuf_addattrib( &mp, MSG_ATTR_OK, 0, NULL );
                enqueue_msg( mp );
            }
        }
        break;
    /* Responses */
    case MSG_TYPE_PING_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            DLOG( "TODO: Handle Ping response (calculate round trip time).\n" );
        }
        break;
    case MSG_TYPE_PEERLIST_RES:
        if ( CLT_AUTH_OK == cfg.st && 0ULL == srcid )
        {
            enum MSG_ATTRIB at2;
            size_t al2;
            void *av2;
            printcon( "Peerlist start\n" );
            while ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
                && MSG_ATTR_PEERID == at
                && 0 == mbuf_getnextattrib( *pp, &at2, &al2, &av2 )
                && MSG_ATTR_PEERNAME == at2 )
            {
                printcon( "%016"PRIx64" %s\n", NTOH64( *(uint64_t *)av ), (char *)av2 );
            }
            printcon( "Peerlist end\n" );
        }
        break;
    case MSG_TYPE_REGISTER_RES:
        if ( CLT_PRE_LOGIN == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( "Registered" );
            cfg.st = CLT_AUTH_OK;
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( ": '%s'\n", (char *)av );
            else
                printcon( "\n" );
        }
        break;
    case MSG_TYPE_LOGIN_RES:
        if ( CLT_PRE_LOGIN == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_CHALLENGE == at )
        {
            printcon( "Login Ok\nAuthenticating\n" );
            cfg.st = CLT_LOGIN_OK;
            mbuf_compose( &mp, MSG_TYPE_AUTH_REQ, 0, 0, trid, ++exid );
            DLOG( "TODO: hashing, crypt, whatever, ...\n" );
            mbuf_addattrib( &mp, MSG_ATTR_DIGEST, al, av );
            enqueue_msg( mp );
        }
        break;
    case MSG_TYPE_AUTH_RES:
        if ( CLT_LOGIN_OK == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( "Authenticated" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( ": '%s'\n", (char *)av );
            else
                printcon( "\n" );
            cfg.st = CLT_AUTH_OK;
        }
        break;
    case MSG_TYPE_LOGOUT_RES:
        if ( ( CLT_LOGIN_OK == cfg.st || CLT_AUTH_OK == cfg.st )
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( "Logged out" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( ": '%s'\n", (char *)av );
            else
                printcon( "\n" );
            cfg.st = CLT_PRE_LOGIN;
        }
        break;
    default:
        if ( MCLASS_IS_ERR( mtype )
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_ERROR == at )
        {
            uint64_t ec = NTOH64( *(uint64_t *)av );
            char *es = NULL;
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                es = (char *)av;
            XLOG( LOG_INFO, "Received error: mtype=0x%04"PRIx16": %"PRIu64"%s%s.\n",
                    mtype, ec, es?" ":"", es?es:"" );
            printcon( "%s Error %"PRIu64"%s%s.\n",
                        mtype2str( mtype ), ec, es?" ":"", es?es:"" );
        }
        else
        {
            printcon( "Unhandled message 0x%04"PRIx16" (%s_%s)!\n",
                        mtype, mtype2str( mtype ), mclass2str( mtype ) );
            XLOG( LOG_WARNING, "Unhandled message 0x%04"PRIx16"!\n" );
            mbuf_dump( *pp );
        }
        break;
    }
    mbuf_free( pp );
    return res;
}

static int handle_srvio( int nset, int *srvfd, fd_set *rfds, fd_set *wfds )
{
    /* Read from server. */
    if ( FD_ISSET( *srvfd, rfds ) )
    {
        --nset;
        /* Prepare receive buffer. */
        if ( NULL == rbuf )
            mbuf_new( &rbuf );

        if ( rbuf->boff < rbuf->bsize )
        {   /* Message buffer not yet filled. */
            int r;
            errno = 0;
            r = read( *srvfd, rbuf->b + rbuf->boff, rbuf->bsize - rbuf->boff );
            if ( 0 > r )
            {
                if ( EAGAIN != errno && EWOULDBLOCK != errno && EINTR != errno )
                {
                    XLOG( LOG_ERR, "read() failed: %m.\n" );
                    goto DISC;
                }
                goto SKIP_TO_WRITE;
            }
            if ( 0 == r )
            {
                DLOG( "Remote host closed connection.\n" );
                goto DISC;
            }
            //DLOG( "%d bytes received.\n", r );
            rbuf->boff += r;
        }
        if ( rbuf->boff == rbuf->bsize )
        {   /* Buffer filled. */
            if ( rbuf->boff == MSG_HDR_SIZE )
            {   /* Only received header yet. */
                uint16_t paylen = HDR_GET_PAYLEN( rbuf );
                if ( paylen > 0 )
                {   /* Prepare for receiving payload next. */
                    //DLOG( "Grow message buffer by %lu.\n", paylen );
                    mbuf_resize( &rbuf, paylen );
                }
                //DLOG( "Expecting %"PRIu16" bytes of payload data.\n", paylen );
            }
            if ( rbuf->boff == rbuf->bsize )
            {   /* Payload data complete. */
                process_srvmsg( &rbuf );
            }
        }
    }
SKIP_TO_WRITE:
    /* Write to server. */
    if ( 0 < nset && FD_ISSET( *srvfd, wfds ) )
    {
        --nset;
        if ( qhead->boff < qhead->bsize )
        {   /* Message buffer not yet fully sent. */
            int w;
            errno = 0;
            w = write( *srvfd, qhead->b + qhead->boff, qhead->bsize - qhead->boff );
            if ( 0 > w )
            {
                if ( EAGAIN != errno && EWOULDBLOCK != errno && EINTR != errno )
                {
                    XLOG( LOG_ERR, "write() failed: %m.\n" );
                    goto DISC;
                }
                goto DONE;
            }
            if ( 0 == w )
            {
                DLOG( "WTF, write() returned 0: %m.\n" );
                goto DISC;
            }
            //DLOG( "%d bytes sent to server.\n", w );
            qhead->boff += w;
        }
        if ( qhead->boff == qhead->bsize )
        {   /* Message sent, remove from queue. */
            dequeue_msg();
        }
    }
DONE:
    return nset;
    
DISC:
    disconnect_srv( srvfd );
    cfg.st = CLT_INVALID;
    printcon( "Disconnected, connection to remote host failed.\n" );
    return nset;
}

int main( int argc, char *argv[] )
{
    int srvfd = -1;

    /* Initialization. */
    XLOG_INIT( argv[0] );
    srandom( time( NULL ) ^ getpid() );
    eval_cmdline( argc, argv );
    signal( SIGPIPE, SIG_IGN );     /* Ceci n'est pas une pipe. */
    /* TODO: ignore or handle other signals? */

    DLOG( "Entering main loop.\n" );
    cfg.st = CLT_INVALID;
    while ( 1 )
    {
        int nset;
        int maxfd;
        fd_set rfds, wfds;
        struct timeval to;

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
            nset = handle_srvio( nset, &srvfd, &rfds, &wfds );
            if ( 0 < nset && FD_ISSET( STDIN_FILENO, &rfds ) )
            {
                --nset;
                if ( 0 == process_stdin( &srvfd ) )
                    break;  /* EOF on stdin terminates client. */
            }
            if ( 0 < nset ) /* Can happen on read errors. */
                XLOG( LOG_INFO, "%d file descriptors still set!\n", nset );
        }
        else if ( 0 == nset )
        {   /* Select timed out */
            if ( CLT_AUTH_OK == cfg.st )
            {   /* Send ping to server. */
                mbuf_t *mb = NULL;
                mbuf_compose( &mb, MSG_TYPE_PING_IND, 0ULL, 0ULL, random(), 1 );
                enqueue_msg( mb );
            }
        }
        else if ( EINTR == errno )
        {
            DLOG( "select() was interrupted: %m.\n" );
        }
        else
        {   /* FATAL ERRORS: */
            /* Unable to allocate memory for internal tables. */
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
    disconnect_srv( &srvfd );
    exit( EXIT_SUCCESS );
}

/* EOF */
