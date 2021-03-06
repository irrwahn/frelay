/*
 * srvmain.c
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#ifndef FD_COPY
    /* According to POSIX fd_set is a structure type! */
    #define FD_COPY(dst,src) (*(dst)=*(src))
#endif

#include <stricmp.h>

#include "auth.h"
#include "cfgparse.h"
#include "message.h"
#include "srvcfg.h"
#include "srvuserdb.h"
#include "util.h"
#include "version.h"


enum CLT_STATE {
    CLT_INVALID = 0,
    CLT_PRE_LOGIN,
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
    char *name;                 /* associated user name */
    char *key;                  /* user key (registered users only) */
    enum CLT_STATE st;          /* client state */
    time_t act;                 /* time of last activity (s since epoch) */
    mbuf_t *rbuf;               /* receive buffer pointer */
    mbuf_t *qhead, *qtail;      /* send buffer queue pointers */
};


/* Configuration singleton */
static struct {
    char *interface;
    char *listenport;
    int select_timeout;
    int msg_timeout;
    int conn_timeout;
    int max_clients;
    int have_config;
    const char *config_path;
    char *userdb_path;
    const char *motd_cmd;
} cfg;

static cfg_parse_def_t cfgdef[] = {
    { "interface",      CFG_PARSE_T_STR, &cfg.interface },
    { "listenport",     CFG_PARSE_T_STR, &cfg.listenport },
    { "select_timeout", CFG_PARSE_T_INT, &cfg.select_timeout },
    { "msg_timeout",    CFG_PARSE_T_INT, &cfg.msg_timeout },
    { "conn_timeout",   CFG_PARSE_T_INT, &cfg.conn_timeout },
    { "max_clients",    CFG_PARSE_T_INT, &cfg.max_clients },
    { "userdb_path",    CFG_PARSE_T_STR, &cfg.userdb_path },
    { "motd_cmd",       CFG_PARSE_T_STR, &cfg.motd_cmd },
    { NULL, CFG_PARSE_T_NONE, NULL }
};


/**********************************************
 * INITIALIZATION
 *
 */

static void print_usage( const char *argv0 )
{
    const char *p = ( NULL == ( p = strrchr( argv0, '/' ) ) ) ? argv0 : p+1;
    fprintf( stderr,
        "Usage: %s [OPTION]... \n"
        "  -c <filename>  : read configuration from specified file\n"
        "  -i <interface> : specify IP address to bind to\n"
        "  -p <port>      : specify TCP listen port\n"
        "  -u <filename>  : specify user database file\n"
        "  -h             : print help text and exit\n"
        "  -v             : print version information and exit\n"
        , p
    );
}

static int eval_cmdline( int argc, char *argv[] )
{
    int opt;
    const char *optstr = ":c:i:p:u:hv";

    opterr = 0;
    errno = 0;
    do
    {
        opt = getopt( argc, argv, optstr );
        switch ( opt )
        {
        case -1:
            break;
        case 'c':
            errno = 0;
            if ( 0 != cfg_parse_file( optarg, cfgdef ) && 0 != errno )
                XLOG( LOG_WARNING, "Reading config file '%s' failed: %s\n",
                        optarg, strerror( errno ) );
            else
                cfg.have_config = 1;
            break;
        case 'i':
            free( cfg.interface );
            cfg.interface = strdup_s( optarg );
            break;
        case 'p':
            free( cfg.listenport );
            cfg.listenport = strdup_s( optarg );
            break;
        case 'u':
            free( cfg.userdb_path );
            cfg.userdb_path = strdup_s( optarg );
            break;
        case 'v':
            fprintf( stderr, "frelay server version %s\n", VERSION );
            exit( EXIT_SUCCESS );
            break;
        case 'h':
            print_usage( argv[0] );
            exit( EXIT_SUCCESS );
            break;
        case '?':
            print_usage( argv[0] );
            XLOG( LOG_WARNING, "Unrecognized option '-%c'\n", optopt );
            exit( EXIT_FAILURE );
            break;
        case ':':
            print_usage( argv[0] );
            XLOG( LOG_WARNING, "Missing argument for option '-%c'\n", optopt );
            exit( EXIT_FAILURE );
            break;
        default:
            print_usage( argv[0] );
            exit( EXIT_FAILURE );
            break;
        }
    }
    while ( -1 != opt );
    return 0;
}

static int init_config( int argc, char *argv[] )
{
    /* Assign build time default values. */
    cfg.interface = strdup_s( DEF_INTERFACE );
    cfg.listenport = strdup_s( DEF_PORT );
    cfg.select_timeout = SEL_TIMEOUT_S;
    cfg.msg_timeout = MSG_TIMEOUT_S;
    cfg.conn_timeout = CONN_TIMEOUT_S;
    cfg.max_clients = MAX_CLIENTS;
    cfg.have_config = 0;
    cfg.config_path = strdup_s( CONFIG_PATH );
    cfg.userdb_path = strdup_s( USERDB_PATH );
    cfg.motd_cmd = strdup_s( MOTD_CMD );

    /* Parse command line options. */
    eval_cmdline( argc, argv );

    if ( !cfg.have_config )
    {   /* Parse default configuration file. */
        if ( 0 != cfg_parse_file( cfg.config_path, cfgdef ) && 0 != errno )
            XLOG( LOG_WARNING, "Reading config file '%s' failed: %s\n",
                    cfg.config_path, strerror( errno ) );
        else
            cfg.have_config = 1;
    }
    return 0;
}

static int init_server( client_t **clients )
{
    int res = 0;
    const char *iface = ( cfg.interface && *cfg.interface ) ? cfg.interface : NULL;
    int fd = -1;
    struct addrinfo hints, *info, *ai;

    /* Initialize clients array. */
    if( (int)FD_SETSIZE < cfg.max_clients )
    {
        XLOG( LOG_WARNING,
            "Maximum number of clients trimmed down to FD_SETZIZE (%d).\n",
            FD_SETSIZE );
        cfg.max_clients = FD_SETSIZE;
    }
    *clients = malloc_s( cfg.max_clients * sizeof **clients );
    memset( *clients, 0, cfg.max_clients * sizeof **clients );
    for ( int i = 0; i < cfg.max_clients; ++i )
        (*clients)[i].fd = -1;

    /* Create socket, set SO_REUSEADDR, bind and listen. */
    memset( &hints, 0, sizeof hints );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    res = getaddrinfo( iface, cfg.listenport, &hints, &info );
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


/**********************************************
 * MOTD STUFF
 *
 */

static char motd[4000] = "Welcome!";
static size_t motd_sz = 0;

static int motd_cb( const char *s )
{
    if ( strlen( s ) < sizeof motd - motd_sz )
        strcpy( motd + motd_sz, s );
    motd_sz += strlen( s );
    return 0;
}

static const char *motd_get( void )
{
    motd_sz = 0;
    pcmd( cfg.motd_cmd, motd_cb );
    return motd;
}


/**********************************************
 * CLIENT PROCESSING
 *
 */

static int close_client( client_t *cp, fd_set *m_rfds, fd_set *m_wfds )
{
    DLOG( "Closing connection to [%s:%hu].\n",
        inet_ntoa( cp->addr.sin_addr ), cp->addr.sin_port );
    FD_CLR( cp->fd, m_rfds );
    FD_CLR( cp->fd, m_wfds );
    close( cp->fd );
    free( cp->name );
    free( cp->key );
    mbuf_free( &cp->rbuf );
    for ( mbuf_t *qp = cp->qhead, *next; NULL != qp; qp = next )
    {
        next = qp->next;
        mbuf_free( &qp );
    }
    memset( cp, 0, sizeof *cp );
    cp->fd = -1;
    cp->st = CLT_INVALID;
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
    if ( (int)FD_SETSIZE <= fd )
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
    clients[i].id = 0ULL;   /* Set upon login. */
    clients[i].name = NULL; /* Set upon login. */
    clients[i].key = NULL;  /* Set upon login. */
    clients[i].st = CLT_PRE_LOGIN;
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
                c[i].st = CLT_INVALID;
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


/**********************************************
 * MESSAGE PROCESSING
 *
 */

static int enqueue_msg( client_t *cp, mbuf_t *m, fd_set *m_wfds )
{
    DLOG( "%p\n", m );
    m->boff = 0;
    m->next = NULL;
    if ( NULL != cp->qtail )
        cp->qtail->next = m;
    cp->qtail = m;
    if ( NULL == cp->qhead )
        cp->qhead = m;
    FD_SET( cp->fd, m_wfds );
    return 0;
}

static int dequeue_msg( client_t *cp, fd_set *m_wfds )
{
    mbuf_t *next;

    DLOG( "dump:\n" );
    mbuf_dump( cp->qhead );

    next = cp->qhead->next;
    if ( cp->qtail == cp->qhead )
        cp->qtail = NULL;
    mbuf_free( &cp->qhead );
    cp->qhead = next;
    if ( NULL == cp->qhead )
        FD_CLR( cp->fd, m_wfds );
    return 0;
}

static int process_server_msg( client_t *c, int i_src, fd_set *m_rfds, fd_set *m_wfds )
{
    uint16_t mtype = HDR_GET_TYPE( c[i_src].rbuf );
    enum MSG_ATTRIB at;
    size_t al;
    void *av;
    const udb_t *pu;

    if ( CLT_AUTH_OK != c[i_src].st
        && MSG_TYPE_LOGIN_REQ != mtype
        && MSG_TYPE_AUTH_REQ != mtype
        && MSG_TYPE_REGISTER_REQ != mtype )
    {
        mbuf_to_error_response( &c[i_src].rbuf, SC_FORBIDDEN );
        return -1;
    }
    mbuf_resetgetattrib( c[i_src].rbuf );
    switch ( mtype )
    {
    case MSG_TYPE_PING_IND:
        /* Nothing to be done - client activity already recorded. */
        mbuf_free( &c[i_src].rbuf );
        break;
    case MSG_TYPE_PING_REQ:
        mbuf_to_response( &c[i_src].rbuf );
        mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_OK, 0, NULL );
        break;
    case MSG_TYPE_REGISTER_REQ:
        DLOG( "Process REGISTER request.\n" );
        if ( CLT_AUTH_OK != c[i_src].st )
        {
            mbuf_to_error_response( &c[i_src].rbuf, SC_UNAUTHORIZED );
        }
        else if ( 0 != mbuf_getnextattrib( c[i_src].rbuf, &at, &al, &av )
                || at != MSG_ATTR_PUBKEY )
        {
            mbuf_to_error_response( &c[i_src].rbuf, SC_BAD_REQUEST );
        }
        else
        {
            char *key;
            key = strdupcat_s( AUTH_KEY_PLAINTEXT, av );
            udb_dropentry( c[i_src].name ); /* Not exactly elegant ... */
            if ( NULL != ( pu = udb_addentry( c[i_src].id, c[i_src].name, key ) ) )
            {
                mbuf_to_response( &c[i_src].rbuf );
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_OK, 0, NULL );
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_NOTICE, sizeof TXT_REGISTERED, TXT_REGISTERED );
            }
            else
                mbuf_to_error_response( &c[i_src].rbuf, SC_LOCKED );
            free( key );
        }
        break;
    case MSG_TYPE_DROP_REQ:
        DLOG( "Process DROP request.\n" );
        if ( CLT_AUTH_OK != c[i_src].st )
        {
            mbuf_to_error_response( &c[i_src].rbuf, SC_UNAUTHORIZED );
        }
        else if ( 0 != udb_dropentry( c[i_src].name ) )
        {
            mbuf_to_error_response( &c[i_src].rbuf, SC_NOT_FOUND );
        }
        else
        {
            mbuf_to_response( &c[i_src].rbuf );
            mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_OK, 0, NULL );
            mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_NOTICE, sizeof TXT_DROPPED, TXT_DROPPED );
        }
        break;
    case MSG_TYPE_LOGIN_REQ:
        DLOG( "WIP: Process LOGIN request.\n" );
        if ( CLT_PRE_LOGIN != c[i_src].st
            || 0 != mbuf_getnextattrib( c[i_src].rbuf, &at, &al, &av )
            || at != MSG_ATTR_USERNAME )
        {
            mbuf_to_error_response( &c[i_src].rbuf, SC_BAD_REQUEST );
        }
        else if ( NULL == ( pu = udb_lookupname( (char *)av ) ) )
        {   /* Login as unregistered user. */
            int i = cfg.max_clients;
            for ( i = 0; i < cfg.max_clients; ++i )
                if ( 0 <= c[i].fd && NULL != c[i].name
                    && 0 == stricmp( c[i].name, (char *)av ) )
                    break;
            if ( i != cfg.max_clients )
            {   /* Name already in use. */
                mbuf_to_error_response( &c[i_src].rbuf, SC_CONFLICT );
            }
            else
            {
                c[i_src].st = CLT_AUTH_OK;
                c[i_src].id = udb_gettempid();
                c[i_src].name = strdup_s( (char *)av );
                c[i_src].key = NULL;
                mbuf_to_response( &c[i_src].rbuf );
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_OK, 0, NULL );
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_NOTICE, strlen( motd ) + 1, motd );
            }
        }
        else
        {   /* Registered user: send challenge. */
            c[i_src].st = CLT_LOGIN_OK;
            c[i_src].id = pu->id;
            c[i_src].name = strdup_s( pu->name );
            /* PROOF OF CONCEPT ONLY (road works ahead): */
            if ( 0 == strncmp( pu->key, AUTH_KEY_PLAINTEXT, strlen( AUTH_KEY_PLAINTEXT ) ) )
            {
                c[i_src].key = strdup_s( pu->key );
                mbuf_to_response( &c[i_src].rbuf );
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_CHALLENGE, strlen( AUTH_KEY_PLAINTEXT ) + 1, AUTH_KEY_PLAINTEXT );
            }
            else
                mbuf_to_error_response( &c[i_src].rbuf, SC_METHOD_NOT_ALLOWED );
        }
        break;
    case MSG_TYPE_AUTH_REQ:
        DLOG( "WIP: Process AUTH request.\n" );
        if ( CLT_LOGIN_OK != c[i_src].st
            || 0 != mbuf_getnextattrib( c[i_src].rbuf, &at, &al, &av )
            || at != MSG_ATTR_DIGEST )
        {
            c[i_src].st = CLT_PRE_LOGIN;
            mbuf_to_error_response( &c[i_src].rbuf, SC_BAD_REQUEST );
            break;
        }
        /* PROOF OF CONCEPT ONLY (road works ahead): */
        if ( 0 != strcmp( (const char *)av, c[i_src].key ) )
        {
            c[i_src].st = CLT_PRE_LOGIN;
            mbuf_to_error_response( &c[i_src].rbuf, SC_UNAUTHORIZED );
            break;
        }
        for ( int i = 0; i < cfg.max_clients; ++i )
            if ( c[i_src].id == c[i].id && i_src != i && 0 <= c[i].fd )
                close_client( &c[i], m_rfds, m_wfds );
        c[i_src].st = CLT_AUTH_OK;
        mbuf_to_response( &c[i_src].rbuf );
        mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_OK, 0, NULL );
        mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_NOTICE, strlen( motd ) + 1, motd );
        break;
    case MSG_TYPE_LOGOUT_REQ:
        DLOG( "Process LOGOUT request.\n" );
        if ( CLT_AUTH_OK != c[i_src].st && CLT_LOGIN_OK != c[i_src].st )
        {
            mbuf_to_error_response( &c[i_src].rbuf, SC_UNAUTHORIZED );
            break;
        }
        c[i_src].st = CLT_PRE_LOGIN;
        c[i_src].id = 0ULL;
        free( c[i_src].name ); c[i_src].name = NULL;
        free( c[i_src].key ); c[i_src].key = NULL;
        mbuf_to_response( &c[i_src].rbuf );
        mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_OK, 0, NULL );
        mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_NOTICE, sizeof TXT_BYE, TXT_BYE );
        break;
    case MSG_TYPE_PEERLIST_REQ:
        DLOG( "Process PEERLIST request.\n" );
        mbuf_to_response( &c[i_src].rbuf );
        for ( int i = 0; i < cfg.max_clients; ++i )
        {
            if ( 0 <= c[i].fd && CLT_AUTH_OK == c[i].st )
            {
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_PEERID, 8, c[i].id );
                mbuf_addattrib( &c[i_src].rbuf, MSG_ATTR_PEERNAME,
                                strlen( c[i].name ) + 1, c[i].name );
            }
        }
        break;
    /* Anything else is nonsense: */
    default:
        XLOG( LOG_INFO, "Message type 0x%04"PRIX16" not supported by server.\n", mtype );
        if ( MCLASS_IS_REQ( mtype ) )
        {
            DLOG( "Add error response to c[%d] send queue.\n", i_src );
            mbuf_to_error_response( &c[i_src].rbuf, SC_I_AM_A_TEAPOT );
        }
        else
        {   /* Non-requests are silently dropped! */
            mbuf_free( &c[i_src].rbuf );
        }
        return -1;
        break;
    }
    return 0;
}

static int process_broadcast_msg( client_t *c, int i_src, fd_set *m_wfds )
{
    uint16_t mtype = HDR_GET_TYPE( c[i_src].rbuf );

    if ( CLT_AUTH_OK != c[i_src].st )
    {
        mbuf_to_error_response( &c[i_src].rbuf, SC_FORBIDDEN );
        return -1;
    }
    switch ( mtype )
    {
    default:
        XLOG( LOG_WARNING, "Broadcast messages not implemented in server.\n" );
        if ( MCLASS_IS_REQ( mtype ) )
        {
            DLOG( "Add error response to c[%d] send queue.\n", i_src );
            mbuf_to_error_response( &c[i_src].rbuf, SC_NOT_IMPLEMENTED );
        }
        else
        {   /* Non-requests are silently dropped! */
            mbuf_free( &c[i_src].rbuf );
        }
        return -1;
        break;
    }
    return 0;
    (void)m_wfds;
}

static int process_forward_msg( client_t *c, int i_src, fd_set *m_wfds )
{
    int i_dst;
    uint16_t mtype = HDR_GET_TYPE( c[i_src].rbuf );

    if ( CLT_AUTH_OK != c[i_src].st )
    {
        mbuf_to_error_response( &c[i_src].rbuf, SC_FORBIDDEN );
        return -1;
    }

    for ( i_dst = 0; i_dst < cfg.max_clients; ++i_dst )
    {
        if ( 0 <= c[i_dst].fd
            && CLT_AUTH_OK == c[i_dst].st
            && HDR_GET_DSTID( c[i_src].rbuf ) == c[i_dst].id )
            break;
    }
    if ( i_dst == cfg.max_clients )
    {
        DLOG( "Add error response to c[%d] send queue.\n", i_src );
        mbuf_to_error_response( &c[i_src].rbuf, SC_MISDIRECTED_REQUEST );
        return -1;
    }
    switch ( mtype )
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
        DLOG( "Forwarding message to c[%d] send queue.\n", i_dst );
        enqueue_msg( &c[i_dst], c[i_src].rbuf, m_wfds );
        c[i_src].rbuf = NULL;
        break;
    /* Anything else is nonsense: */
    default:
        XLOG( LOG_WARNING, "Message type 0x%04"PRIX16" not forwarded to client.\n", mtype );
        if ( MCLASS_IS_REQ( mtype ) )
        {
            DLOG( "Add error response to c[%d] send queue.\n", i_src );
            mbuf_to_error_response( &c[i_src].rbuf, SC_BAD_REQUEST );
        }
        else
        {   /* Non-requests are silently dropped! */
            mbuf_free( &c[i_src].rbuf );
        }
        return -1;
        break;
    }
    return 0;
}

static int process_msg( client_t *c, int i_src, fd_set *m_rfds, fd_set *m_wfds )
{
    int r;
    uint64_t dstid = HDR_GET_DSTID( c[i_src].rbuf );

    /* Fill in the *real* client ID! */
    if ( CLT_LOGIN_OK == c[i_src].st || CLT_AUTH_OK == c[i_src].st )
        HDR_SET_SRCID( c[i_src].rbuf, c[i_src].id );
    DLOG( "dump:\n" );
    mbuf_dump( c[i_src].rbuf );

    if ( 0ULL == dstid )
        r = process_server_msg( c, i_src, m_rfds, m_wfds );
    else if ( ~0ULL == dstid )
        r = process_broadcast_msg( c, i_src, m_wfds );
    else
        r = process_forward_msg( c, i_src, m_wfds );
    /* Send back the response, if any: */
    if ( NULL != c[i_src].rbuf )
    {
        enqueue_msg( &c[i_src], c[i_src].rbuf, m_wfds );
        c[i_src].rbuf = NULL;
    }
    return r;
}


/**********************************************
 * I/O HANDLING AND MAIN()
 *
 */

static int handle_io( client_t *c, int nset,
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
                mbuf_new( &c[i].rbuf );
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
                    uint16_t paylen = HDR_GET_PAYLEN( c[i].rbuf );
                    if ( paylen > 0 )
                    {   /* Prepare for receiving payload next. */
                        DLOG( "Grow message buffer by %lu.\n", paylen );
                        mbuf_resize( &c[i].rbuf, paylen );
                    }
                    DLOG( "Expecting %"PRIu16" bytes of payload data.\n", paylen );
                }
                if ( c[i].rbuf->boff == c[i].rbuf->bsize )
                {   /* Payload data complete. */
                    process_msg( c, i, m_rfds, m_wfds );
                    c[i].rbuf = NULL;
                }
            }
        }
    SKIP_TO_WRITE:
        /* Handle fds ready for writing. */
        if ( FD_ISSET( c[i].fd, wfds ) )
        {
            --nset;
            c[i].act = now;
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
                dequeue_msg( &c[i], m_wfds );
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
    struct timeval to_sav;

    /* Initialization. */
    die_if( 0 == getuid() || 0 == geteuid() || 0 == getgid() || 0 == getegid(),
        "%s started with root privileges, aborting!\n", argv[0] );
    signal( SIGPIPE, SIG_IGN );     /* Ceci n'est pas une pipe. */
    /* TODO: gracefully handle termination signals (SIGINT, SIGQUIT, SIGTERM)? */
    XLOG_INIT( argv[0], LOG_TO_SYSLOG|LOG_TO_FILE, stderr );
    init_config( argc, argv );
    udb_init( cfg.userdb_path );
    to_sav = (struct timeval){ .tv_sec = cfg.select_timeout, 0 };

    /* Bring up the server. */
    listenfd = init_server( &clients );
    FD_ZERO( &m_rfds );
    FD_ZERO( &m_wfds );
#ifdef DEBUG
    FD_SET( STDIN_FILENO, &m_rfds );
#endif
    FD_SET( listenfd, &m_rfds );
    DLOG( "Entering main loop.\n" );
    puts( "" ); /* May serve as a "service ready" signal for a supervisor. */
    while ( 1 )
    {
        static time_t last_upkeep = 0;
        time_t now;
        int nset;
        fd_set rfds, wfds;
        struct timeval to;

        now = time( NULL );
        if ( now - last_upkeep > to_sav.tv_sec )
        {   /* Avoid doing upkeep continuously under load. */
            last_upkeep = now;
            maxfd = listenfd;
            upkeep( clients, &maxfd, &m_rfds, &m_wfds );
            motd_get();
        }
        FD_COPY( &rfds, &m_rfds );
        FD_COPY( &wfds, &m_wfds );
        to = to_sav;
        nset = select( maxfd + 1, &rfds, &wfds, NULL, &to );
        if ( 0 < nset )
        {
            /* DLOG( "%d fds ready.\n", nset ); */
            nset = handle_io( clients, nset, &rfds, &wfds, &m_rfds, &m_wfds );
            if ( 0 < nset && 0 <= listenfd && FD_ISSET( listenfd, &rfds ) )
            {
                --nset;
                accept_client( clients, listenfd, &maxfd, &m_rfds );
            }
#ifdef DEBUG
            if ( 0 < nset && FD_ISSET( STDIN_FILENO, &rfds ) )
            {
                --nset;
                if ( 0 == drain_fd( STDIN_FILENO ) )
                    break;  /* EOF on stdin terminates server. */
            }
#endif
            if ( 0 < nset ) /* Can happen on read errors. */
                XLOG( LOG_INFO, "%d file descriptors still set!\n", nset );
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
    /* Never reached during normal (non-debug) operation. */
    XLOG( LOG_INFO, "Terminating.\n" );
    exit( EXIT_SUCCESS );
}

/* EOF */
