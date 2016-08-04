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

#include <libgen.h>     /* basename */
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "message.h"
#include "cltcfg.h"
#include "transfer.h"
#include "util.h"

#include <ntime.h>
#include <prng.h>
#include <stricmp.h>


#define MAX_DATA_SIZE   (MSG_MAX_PAY_SIZE-24)


enum CLT_STATE {
    CLT_INVALID = 0,
    CLT_PRE_LOGIN,
    CLT_LOGIN_OK,
    CLT_AUTH_OK
};

/* Configuration singleton. */
static struct {
    const char *host;
    const char *port;
    struct timeval select_timeout;
    int msg_timeout;
    int res_timeout;
    int offer_timeout;
    const char *username;
    const char *pubkey;
    const char *privkey;
    enum CLT_STATE st;
} cfg = {
    /* .host = */
    NULL,
    /* .port = */
    DEFAULT_PORT,
    /* .select_timeout = */
    { .tv_sec=SELECT_TIMEOUT_MS/1000, .tv_usec=SELECT_TIMEOUT_MS%1000*1000 },
    /* .msg_timeout = */
    MESSAGE_TIMEOUT_S,
    /* .res_timeout = */
    RESPONSE_TIMEOUT_S,
    /* .offer_timeout = */
    OFFER_TIMEOUT_S,
    /* .username = */
    NULL,
    /* .pubkey = */
    NULL,
    /* .privkey = */
    NULL,
    /* .st = */
    CLT_INVALID,
};


/**********************************************
 * INITIALIZATION
 *
 */

static int eval_cmdline( int argc, char *argv[] )
{
    if ( 1 < argc )
        cfg.host = argv[1];
    if ( 2 < argc )
        cfg.port = argv[2];
    return 0;
}


/**********************************************
 * CONSOLE STUFF
 *
 */

static void prompt( int on )
{
    if ( isatty( STDIN_FILENO ) )
        fprintf( stderr, on ? "\rfrelay> " :"\r       \r" );
}

/*
 * Just to have a central function to control console output.
 */
static int printcon( const char *fmt, ... )
{
    int r;
    va_list arglist;
    FILE *ofp = stdout;

    prompt( 0 );
    va_start( arglist, fmt );
    r = vfprintf( ofp, fmt, arglist );
    va_end( arglist );
    fflush( ofp );
#ifdef DEBUG
    va_start( arglist, fmt );
    XLOGV( LOG_DEBUG, fmt, arglist );
    va_end( arglist );
#endif
    prompt( 1 );
    return r;
}

static int putscon_cb( const char *s )
{
    return printcon( "%s", s );
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
        const char *emsg = EAI_SYSTEM != res ? gai_strerror( res ) : strerror( errno );
        XLOG( LOG_WARNING, "getaddrinfo(%s,%s) failed: %s\n", host, service, emsg );
        printcon( "%s:%s: %s\n", host, service, emsg );
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
                XLOG( LOG_WARNING, "connect() failed: %m.\n" );
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
        printcon( "%s:%s: %s\n", host, service, strerror( errno ) );
        return -1;
    }
    DLOG( "Connected to [%s:%s].\n", host, service );
    printcon( "Connected to %s:%s\n", host, service );
    if ( 0 != set_nonblocking( fd ) )
        XLOG( LOG_WARNING, "set_nonblocking() failed: %m.\n" );
    if ( 0 != set_cloexec( fd ) )
        XLOG( LOG_WARNING, "set_cloexec() failed: %m.\n" );
    *pfd = fd;
    return 0;
}


/**********************************************
 * MESSAGE AND PENDING REQUESTS PROCESSING
 *
 */

/* Receive buffer. */
static mbuf_t *rbuf = NULL;
/* Send queue. */
static mbuf_t *qhead = NULL, *qtail = NULL;
/* List of requests pending a response. */
static mbuf_t *requests = NULL;

/* Add message to send queue. */
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

/* Remove message from send queue. */
static int dequeue_msg( void )
{
    mbuf_t *next;

    //DLOG( "\n" ); mbuf_dump( qhead );
    next = qhead->next;
    if ( qtail == qhead )
        qtail = NULL;
    if ( HDR_CLASS_IS_REQ( qhead ) )
    {   /* Move request to pending list. */
        //DLOG( "Move to pending:\n" ); mbuf_dump( qhead );
        qhead->next = requests;
        requests = qhead;
    }
    else
        mbuf_free( &qhead );
    qhead = next;
    return 0;
}

/* Find, un-list and return a pending request matching the supplied response. */
static mbuf_t *match_pending_req( const mbuf_t *m )
{
    mbuf_t *q;
    mbuf_t *prev = NULL;

    for ( q = requests; NULL != q; q = q->next )
    {
        if (   HDR_GET_DSTID(q) == HDR_GET_SRCID(m)
            && HDR_GET_TRFID(q) == HDR_GET_TRFID(m)
            && MCLASS_TO_RES( HDR_GET_TYPE(q) ) == HDR_GET_TYPE(m) )
        {
            if ( prev )
                prev->next = q->next;
            else
                requests = q->next;
            q->next = NULL;
            break;
        }
        prev = q;
    }
    return q;
}

/* clear out the pending request and offer lists. */
static void upkeep_pending( void )
{
    mbuf_t *q, *next, *prev;
    time_t now = time( NULL );

    next = prev = NULL;
    for ( q = requests; NULL != q; q = next )
    {
        next = q->next;
        if ( (time_t)ntime_to_s( HDR_GET_TS(q) ) + cfg.res_timeout < now )
        {   /* Request passed best before date, trash it. */
            DLOG( "Request timed out:\n" ); mbuf_dump( q );
            if ( prev )
                prev->next = next;
            else
                requests = next;
            mbuf_free( &q );
        }
        else
            prev = q;
    }
    return;
}


/**********************************************
 * I/O HANDLING AND MAIN()
 *
 */

static int process_stdin( int *srvfd )
{
#define MAX_ARG     10
#define MAX_CMDLINE 4000
    mbuf_t *mp = NULL;
    int r;
    int a;
    char *cp;
    const char *arg[MAX_ARG] = { NULL };
    static char line[MAX_CMDLINE];
    static char aline[MAX_CMDLINE];

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
    strcpy( aline, line ); /* backup copy of command line */

    /* Ye lousy tokeniz0r. */
    cp = line;
    while ( *cp && isspace( (unsigned char)*cp ) )
        ++cp;
    for ( a = 0; *cp && a < MAX_ARG; ++a )
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
    if ( NULL == arg[0] || '\0' == arg[0][0] )
    {
        printcon( "" );
        return -1;
    }
    aline[ arg[a-1] - arg[0] + strlen( arg[a-1] ) ] = '\0';

    if ( MATCH_CMD( "ping" ) )
    {   /* ping [destination [notice]] */
        mbuf_compose( &mp, MSG_TYPE_PING_REQ, 0,
                ( 1 < a ) ? strtoull( arg[1], NULL, 16 ) : 0, random() );
        if ( 2 < a )
        {
            cp = aline + ( arg[2] - arg[0] );
            mbuf_addattrib( &mp, MSG_ATTR_NOTICE, strlen( cp ) + 1, cp );
        }
    }
    else if ( MATCH_CMD( "peerlist" ) || MATCH_CMD( "who" ) )
    {   /* peerlist */
        mbuf_compose( &mp, MSG_TYPE_PEERLIST_REQ, 0, 0, random() );
    }
    else if ( MATCH_CMD( "offer" ) )
    {   /* offer destination file [notice] */
        const transfer_t *o;
        char *bname, *fname;

        if ( 3 > a )
        {
            printcon( "Usage: offer destination file [notice]\n" );
            return -1;
        }
        if ( NULL == ( o = offer_new( strtoull( arg[1], NULL, 16 ), arg[2] ) ) )
        {
            printcon( "No such file: '%s'\n", arg[2] );
            return -1;
        }
        mbuf_compose( &mp, MSG_TYPE_OFFER_REQ, 0, o->rid, random() );
        mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, o->oid );
        fname = strdup( arg[2] );
        bname = basename( fname );
        mbuf_addattrib( &mp, MSG_ATTR_FILENAME, strlen( bname ) + 1, bname );
        free( fname );
        mbuf_addattrib( &mp, MSG_ATTR_SIZE, 8, o->size );
        //TODO MSG_ATTR_FILEHASH ?
        //TODO MSG_ATTR_TTL ?
        if ( 3 < a )
        {
            cp = aline + ( arg[3] - arg[0] );
            mbuf_addattrib( &mp, MSG_ATTR_NOTICE, strlen( cp ) + 1, cp );
        }
    }
    else if ( MATCH_CMD( "accept" ) )
    {   /* accept offer_id */
        uint64_t oid = strtoull( arg[1], NULL, 16 );
        transfer_t *d;

        if ( 2 > a )
        {
            printcon( "Usage: accept offer_id\n" );
            return -1;
        }
        if ( NULL == ( d = download_match( oid ) ) )
        {
            printcon( "Invalid offer ID %"PRIx64"\n", oid );
            return -1;
        }
        d->act = time( NULL );
        if ( 0 == download_resume( d ) )
            printcon( "Download resumed at offset %"PRIu64".\n", d->offset );
        else if ( 0 == download_write( d, NULL, 0 ) )
            printcon( "Download started\n" );
        else
        {
            printcon( "Download failed to start: %s\n", strerror( errno ) );
            return -1;
        }
        mbuf_compose( &mp, MSG_TYPE_GETFILE_REQ, 0, d->rid, random() );
        mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, oid );
        mbuf_addattrib( &mp, MSG_ATTR_OFFSET, 8, d->offset );
        mbuf_addattrib( &mp, MSG_ATTR_SIZE, 8,
                        d->size < MAX_DATA_SIZE ? d->size : MAX_DATA_SIZE );
    }
    else if ( MATCH_CMD( "connect" ) || MATCH_CMD( "open" ) )
    {   /* connect [host [port]] */
        if ( 3 > a )
            arg[2] = cfg.port;
        if ( 2 > a && NULL == ( arg[1] = cfg.host ) )
        {
            printcon( "Usage: connect [host [port]]\n" );
            return -1;
        }
        connect_srv( srvfd, arg[1], arg[2] );
        if ( 0 > *srvfd )
        {
            cfg.st = CLT_INVALID;
            return -1;
        }
        cfg.st = CLT_PRE_LOGIN;
    }
    else if ( MATCH_CMD( "disconnect" ) || MATCH_CMD( "close" ) )
    {
        if ( 0 <= *srvfd )
        {
            transfer_closeall();
            DLOG( "Closing connection.\n" );
            disconnect_srv( srvfd );
            printcon( "Connection closed\n" );
        }
        cfg.st = CLT_INVALID;
    }
    else if ( MATCH_CMD( "register" ) )
    {   /* register user pubkey */
        DLOG( "Registering.\n" );
        if ( 2 > a )
        {
            printcon( "Usage: register pubkey\n" );
            return -1;
        }
        mbuf_compose( &mp, MSG_TYPE_REGISTER_REQ, 0, 0, random() );
        mbuf_addattrib( &mp, MSG_ATTR_PUBKEY, strlen( arg[1] ) + 1, arg[1] );
    }
    else if ( MATCH_CMD( "login" ) )
    {   /* login [user] */
        DLOG( "Logging in.\n" );
        if ( 2 > a  && NULL == ( arg[1] = cfg.username ) )
        {
            printcon( "Usage: login username\n" );
            return -1;
        }
        mbuf_compose( &mp, MSG_TYPE_LOGIN_REQ, 0, 0, random() );
        mbuf_addattrib( &mp, MSG_ATTR_USERNAME, strlen( arg[1] ) + 1, arg[1] );
    }
    else if ( MATCH_CMD( "logout" ) )
    {
        DLOG( "Logging out.\n" );
        mbuf_compose( &mp, MSG_TYPE_LOGOUT_REQ, 0, 0, random() );
    }
    else if ( MATCH_CMD( "list" ) )
    {
        printcon( "Transfer list start\n" );
        transfer_list( putscon_cb );
        printcon( "Transfer list end\n" );
    }
    else if ( MATCH_CMD( "remove" ) || MATCH_CMD( "delete" ) )
    {
        int n = 0;
        if ( 2 > a )
        {
            printcon( "Usage: remove type|remote_id|offer_id\n" );
            return -1;
        }
        n = transfer_remove( arg[1] );
        if ( 0 < n )
            printcon( "Removed %d transfer%s\n", n, 1 < n ? "s" : "" );
        else
            printcon( "Removing %s failed\n", arg[1] );
    }
    else if ( MATCH_CMD( "exit" ) || MATCH_CMD( "quit" ) || MATCH_CMD( "bye" ) )
    {
        disconnect_srv( srvfd );
        cfg.st = CLT_INVALID;
        return 0;
    }
    else if ( MATCH_CMD( "cd" ) || MATCH_CMD( "lcd" ) )
    {   /* cd [path] */
        if ( 1 < a )
        {
            cp = aline + ( arg[1] - arg[0] );
            errno = 0;
            if ( 0 != chdir( cp ) )
                printcon( "%s\n", strerror( errno ) );
        }
        printcon( "Local directory now %s\n", getcwd( line, sizeof line ) );
        return 1;
    }
    else if ( MATCH_CMD( "cmd" ) || MATCH_CMD( "^" ) )
    {
        if ( 2 > a )
        {
            printcon( "Usage: cmd \"command\"\n" );
            return -1;
        }
        cp = aline + ( arg[1] - arg[0] );
        pcmd( cp, putscon_cb );
        prompt( 1 );
    }
    else if ( MATCH_CMD( "shell" ) || MATCH_CMD( "!" ) )
    {
        pid_t pid = fork();
        if ( 0 == pid )
        {
            execvp( DEFAULT_SHELL[0], DEFAULT_SHELL );
            XLOG( LOG_WARNING, "execvp(%s) failed: %m\n", DEFAULT_SHELL[0] );
            exit( EXIT_FAILURE );
        }
        else if ( 0 < pid )
            waitpid( pid, NULL, 0 );
        else
        {
            XLOG( LOG_WARNING, "fork() failed: %m\n" );
            return -1;
        }
        printcon( "" );
        return 1;
    }
    else if ( MATCH_CMD( "help" ) || MATCH_CMD( "?" ) )
    {
        printcon( "Commands may be abbreviated.  Commands are:\n\n"
            "  ?|help\n"
            "  !|sh\n"
            "  ^|cmd ext_command [args]\n"
            "  accept offer_id\n"
            "  cd|lcd\n"
            "  connect|open [host [port]]\n"
            "  disconnect|close\n"
            "  exit|quit|bye\n"
            "  list\n"
            "  login [user_name]\n"
            "  logout\n"
            "  offer peer_id filename [\"text message\"]\n"
            "  peerlist|who\n"
            "  ping [peer_id [\"text message\"]]\n"
            "  pwd\n"
            "  register key\n"
            "  remove|delete type,remote_id,offer_id\n"
            "\n" );
        return 1;
    }
    else
    {
        printcon( "Invalid command. Enter 'help' to get a list of valid commands.\n" );
        DLOG( "unknown command sequence: \"%s\"\n", aline );
    }
#undef MATCH_CMD

    if ( 0 > *srvfd )
    {
        printcon( "Not connected\n" );
        mbuf_free( &mp );
    }
    else if ( NULL != mp )
        enqueue_msg( mp );
    return r;
}

static int process_srvmsg( mbuf_t **pp )
{
    uint16_t mtype = HDR_GET_TYPE( *pp );
    uint64_t srcid = HDR_GET_SRCID( *pp );
    uint64_t trfid = HDR_GET_TRFID( *pp );
    mbuf_t *mp = NULL;
    mbuf_t *qmatch = NULL;
    enum MSG_ATTRIB at, at2;
    size_t al, al2;
    void *av, *av2;
    int res = 0;
    enum SC_ENUM status = SC_OK;

    DLOG( "Received %s_%s from %016"PRIx64".\n", mtype2str( mtype ), mclass2str( mtype ), srcid );
    if ( MCLASS_IS_RES( mtype ) && NULL == ( qmatch = match_pending_req( *pp ) ) )
    {
        DLOG( "Dropping unsolicited response:\n" );
        mbuf_dump( *pp );
        goto DONE;
    }
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
                s = av;
            printcon( "Ping from %016"PRIx64"%s%s%s\n",
                        srcid, s?": '":"", s?s:"", s?"'":"" );
            if ( MCLASS_IS_REQ( mtype ) )
            {
                mbuf_compose( &mp, MSG_TYPE_PING_RES, 0, srcid, trfid );
                mbuf_addattrib( &mp, MSG_ATTR_OK, 0, NULL );
            }
        }
        break;
    case MSG_TYPE_OFFER_REQ:
        if ( CLT_AUTH_OK == cfg.st )
        {
            transfer_t *d = download_new();
            d->rid = srcid;
            while ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) )
            {
                switch ( at )
                {
                case MSG_ATTR_OFFERID:
                    d->oid = NTOH64( *(uint64_t *)av );
                    break;
                case MSG_ATTR_FILENAME:
                    d->name = strdup( av );
                    d->partname = strdupcat( av, ".part" );
                    break;
                case MSG_ATTR_SIZE:
                    d->size = NTOH64( *(uint64_t *)av );
                    break;
                // TODO: FILEHASH, TTL, NOTICE
                default:
                    break;
                }
            }
            if ( NULL == d->name || 0 == d->size )
            {
                d->oid = 0;
                d->act = 0;
                status = SC_BAD_REQUEST;
                break;
            }
            printcon( "Offer from %016"PRIx64": %016"PRIx64" '%s' (%"PRIu64" Bytes)\n",
                        d->rid, d->oid, d->name, d->size );
            mbuf_compose( &mp, MSG_TYPE_OFFER_RES, 0, srcid, trfid );
            mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, d->oid );
            mbuf_addattrib( &mp, MSG_ATTR_OK, 0, NULL );
        }
        break;
    case MSG_TYPE_GETFILE_REQ:
        if ( CLT_AUTH_OK == cfg.st
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OFFERID == at )
        {
            uint64_t oid = NTOH64( *(uint64_t *)av );
            uint64_t offset = 0;
            uint64_t size = 0;
            transfer_t *o;
            void *data = NULL;

            if ( NULL == ( o = offer_match( oid, srcid ) ) )
            {
                status = SC_NOT_FOUND;
                break;
            }
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_OFFSET == at )
                offset = NTOH64( *(uint64_t *)av );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_SIZE == at )
                size = NTOH64( *(uint64_t *)av );
            if ( 0 == size )
            {   /* Remote side signaled 'download finished'. */
                printcon( "Finished uploading %016"PRIx64" '%s'\n", o->oid, o->name );
                transfer_invalidate( o );
            }
            else if ( NULL == ( data = offer_read( o, offset, &size ) ) )
            {
                status = SC_RANGE_NOT_SATISFIABLE;
                break;
            }
            o->offset = offset;
            mbuf_compose( &mp, MSG_TYPE_GETFILE_RES, 0, srcid, trfid );
            mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, oid );
            mbuf_addattrib( &mp, MSG_ATTR_DATA, size, data );
            free( data );
            if ( 0 < size )
                printcon( "Sending %"PRIu64" bytes of %016"PRIx64" '%s' %3"PRIu64"%% (%"PRIu64"/%"PRIu64")\n",
                        size, o->oid, o->name, (o->offset+size)*100/o->size, o->offset + size, o->size );
        }
        break;

    /* Responses */
    case MSG_TYPE_PING_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            ntime_t rtt = ntime_to_us( ntime_get() - HDR_GET_TS( qmatch ) );
            printcon( "Ping response from %016"PRIx64", RTT=%"PRI_ntime".%"PRI_ntime"ms\n",
                        srcid, rtt/1000, rtt%1000 );
        }
        break;
    case MSG_TYPE_OFFER_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OFFERID == at )
        {
            uint64_t oid = NTOH64( *(uint64_t *)av );
            transfer_t *o;
            if ( NULL == ( o = offer_match( oid, srcid ) ) )
            {
                printcon( "Peer %016"PRIx64" referenced invalid offer %016"PRIx64"\n", srcid, oid );
                break;
            }
            printcon( "Peer %016"PRIx64" received offer %016"PRIx64" '%s'\n", srcid, o->oid, o->name );
        }
        break;
    case MSG_TYPE_GETFILE_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OFFERID == at )
        {
            uint64_t oid = NTOH64( *(uint64_t *)av );
            transfer_t *d = download_match( oid );
            if ( 0 != mbuf_getnextattrib( *pp, &at, &al, &av )
                || MSG_ATTR_DATA != at
                || NULL == d )
            {
                DLOG( "Broken GETFILE response!\n" );
            }
            else if ( 0 == al )
            {
                if ( d->offset == d->size )
                    printcon( "Finished downloading %016"PRIx64" '%s'\n", d->oid, d->name );
                else
                    printcon( "Peer signaled premature EOF uploading %016"PRIx64" '%s'\n", d->oid, d->name );
                if ( 0 <= d->fd )
                    transfer_invalidate( d );
                if ( 0 != rename( d->partname, d->name ) )
                    printcon( "Moving '%s' to '%s' failed: %s\n", d->partname, d->name, strerror( errno ) );
            }
            else if ( 0 != download_write( d, av, al ) )
            {
                printcon( "Writing to temporary file '%s' failed, aborted\n", d->partname );
            }
            else
            {
                d->offset += al;
                uint64_t sz = d->size - d->offset;
                mbuf_compose( &mp, MSG_TYPE_GETFILE_REQ, 0, d->rid, random() );
                mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, oid );
                mbuf_addattrib( &mp, MSG_ATTR_OFFSET, 8, d->offset );
                mbuf_addattrib( &mp, MSG_ATTR_SIZE, 8, sz < MAX_DATA_SIZE ? sz : MAX_DATA_SIZE );
                printcon( "Received %zu bytes of %016"PRIx64" '%s' %3"PRIu64"%% (%"PRIu64"/%"PRIu64")\n",
                            al, d->oid, d->name, d->offset*100/d->size, d->offset, d->size );
            }
        }
        break;
    case MSG_TYPE_PEERLIST_RES:
        if ( CLT_AUTH_OK == cfg.st && 0ULL == srcid )
        {
            printcon( "Peer list start\n" );
            while ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
                && MSG_ATTR_PEERID == at
                && 0 == mbuf_getnextattrib( *pp, &at2, &al2, &av2 )
                && MSG_ATTR_PEERNAME == at2 )
            {
                printcon( "%016"PRIx64"%s %s\n", NTOH64( *(uint64_t *)av ), (HDR_GET_DSTID(*pp)==NTOH64( *(uint64_t *)av ))?"*":"",  (char *)av2  );
            }
            printcon( "Peer list end\n" );
        }
        break;
    case MSG_TYPE_REGISTER_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( "Registered\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( "%s\n", (char *)av );
        }
        break;
    case MSG_TYPE_LOGIN_RES:
        if ( CLT_PRE_LOGIN == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && ( MSG_ATTR_OK == at || MSG_ATTR_CHALLENGE == at ) )
        {
            printcon( "Login Ok\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at2, &al2, &av2 ) && MSG_ATTR_NOTICE == at2 )
                printcon( "%s\n", (char *)av2 );
            if ( MSG_ATTR_OK == at )
            {   /* Unregistered user. */
                cfg.st = CLT_AUTH_OK;
            }
            else if ( MSG_ATTR_CHALLENGE == at )
            {   /* Registered user: authenticate. */
                cfg.st = CLT_LOGIN_OK;
                printcon( "Authenticating\n" );
                mbuf_compose( &mp, MSG_TYPE_AUTH_REQ, 0, 0, trfid );
                DLOG( "TODO: hashing, crypt, whatever, ...\n" );
                mbuf_addattrib( &mp, MSG_ATTR_DIGEST, al, av );
            }
        }
        break;
    case MSG_TYPE_AUTH_RES:
        if ( CLT_LOGIN_OK == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( "Authenticated\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( "%s\n", (char *)av );
            cfg.st = CLT_AUTH_OK;
        }
        break;
    case MSG_TYPE_LOGOUT_RES:
        if ( ( CLT_LOGIN_OK == cfg.st || CLT_AUTH_OK == cfg.st )
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( "Logged out\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( "%s\n", (char *)av );
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
                es = av;
            DLOG( "Received error: mtype=0x%04"PRIx16": %"PRIu64"%s%s.\n",
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
DONE:
    mbuf_free( &qmatch );
    if ( SC_OK != status )
    {
        mbuf_to_error_response( pp, status );
        enqueue_msg( *pp );
        *pp = NULL;
        mbuf_free( &mp );
    }
    else
    {
        if ( NULL != mp )
            enqueue_msg( mp );
        mbuf_free( pp );
    }
    return res;
}

static int handle_srvio( int nset, int *srvfd, fd_set *rfds, fd_set *wfds )
{
    if ( 0 > *srvfd )
        goto DONE;
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
                {   /* Prepare to receive payload. */
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
    transfer_closeall();
    disconnect_srv( srvfd );
    cfg.st = CLT_INVALID;
    printcon( "Connection to remote host failed\n" );
    return nset;
}

int main( int argc, char *argv[] )
{
    int srvfd = -1;

    /* Initialization. */
    XLOG_INIT( argv[0] );
    signal( SIGPIPE, SIG_IGN );     /* Ceci n'est pas une pipe. */
    //signal( SIGINT, SIG_IGN );      /* Ctrl-C no more. */
    /* TODO: ignore or handle other signals? */
    srandom( time( NULL ) ^ getpid() );
    eval_cmdline( argc, argv );
    cfg.username = getenv( "USER" );
    cfg.st = CLT_INVALID;
    if ( NULL != cfg.host )
    {
        connect_srv( &srvfd, cfg.host, cfg.port );
        if ( 0 <= srvfd )
            cfg.st = CLT_PRE_LOGIN;
    }
    DLOG( "Entering main loop.\n" );
    printcon( "" );
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
                mbuf_compose( &mb, MSG_TYPE_PING_IND, 0ULL, 0ULL, random() );
                enqueue_msg( mb );
            }
            upkeep_pending();
            transfer_upkeep( cfg.offer_timeout );
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
    DLOG( "Terminating\n" );
    //prompt( 0 );
    transfer_closeall();
    disconnect_srv( &srvfd );
    exit( EXIT_SUCCESS );
}

/* EOF */
