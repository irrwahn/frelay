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
#include <stdbool.h>
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

#include "auth.h"
#include "cfgparse.h"
#include "message.h"
#include "cltcfg.h"
#include "transfer.h"
#include "util.h"
#include "version.h"

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
    char *host;
    char *port;
    int select_timeout;
    int msg_timeout;
    int resp_timeout;
    int offer_timeout;
    char *username;
    char *pubkey;
    char *privkey;
    int no_clobber;
    int autoconnect;
    enum CLT_STATE st;
    bool istty[3];
} cfg;

static cfg_parse_def_t cfgdef[] = {
    { "host",           CFG_PARSE_T_STR, &cfg.host },
    { "port",           CFG_PARSE_T_STR, &cfg.port },
    { "select_timeout", CFG_PARSE_T_INT, &cfg.select_timeout },
    { "msg_timeout",    CFG_PARSE_T_INT, &cfg.msg_timeout },
    { "res_timeout",    CFG_PARSE_T_INT, &cfg.resp_timeout },
    { "offer_timeout",  CFG_PARSE_T_INT, &cfg.offer_timeout },
    { "username",       CFG_PARSE_T_STR, &cfg.username },
    { "pubkey",         CFG_PARSE_T_STR, &cfg.pubkey },
    { "privkey",        CFG_PARSE_T_STR, &cfg.privkey },
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
        "Usage: %s [OPTION]... [hostname [port]] [OPTION]... \n"
        "  -a            : connect automatically on startup\n"
        "  -c <filename> : read configuration from specified file\n"
        "  -w <filename> : write configuration to specified file\n"
        "  -p <string>   : set password\n"
        "  -u <string>   : set username\n"
        "  -n            : no clobber (do not overwrite files)\n"
        "  -h            : print help text and exit\n"
        "  -v            : print version information and exit\n"
        , p
    );
}

static int eval_cmdline( int argc, char *argv[] )
{
    int opt, nonopt = 0, r;
    const char *optstr = ":c:p:u:w:ahnv";

    opterr = 0;
    errno = 0;
    do
    {
        opt = getopt( argc, argv, optstr );
        switch ( opt )
        {
        case -1:
            if ( optind >= argc )
                break;
            opt = 0;
            if ( 0 == nonopt )
            {
                ++nonopt;
                free( cfg.host );
                cfg.host = strdup_s( argv[optind++] );
            }
            else if ( 1 == nonopt )
            {
                ++nonopt;
                free( cfg.port );
                cfg.port = strdup_s( argv[optind++] );
            }
            else
            {
                print_usage( argv[0] );
                fprintf( stderr, "Excess non-option argument: '%s'\n", argv[optind++] );
                exit( EXIT_FAILURE );
            }
            break;
        case 'c':
            errno = 0;
            if ( 0 != ( r = cfg_parse_file( optarg, cfgdef ) ) && 0 != errno )
                XLOG( LOG_WARNING, "Reading config file '%s' failed: %s\n", optarg, strerror( errno ) );
            break;
        case 'w':
            errno = 0;
            if ( 0 != ( r = cfg_write_file( optarg, cfgdef ) ) && 0 != errno )
                XLOG( LOG_WARNING, "Writing config file '%s' failed: %s\n", optarg, strerror( errno ) );
            break;
        case 'p':
            free( cfg.pubkey );
            cfg.pubkey = strdup_s( optarg );
            break;
        case 'u':
            free( cfg.username );
            cfg.username = strdup_s( optarg );
            break;
        case 'a':
            cfg.autoconnect = 1;
            break;
        case 'n':
            cfg.no_clobber = 1;
            break;
        case 'v':
            fprintf( stderr, "frelay client version %s-%s\n", VERSION, SVNVER );
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
        case 'h':
            print_usage( argv[0] );
            exit( EXIT_SUCCESS );
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
    int r = 0;
    char *tmp;
    char *cfg_filename;

    /* Assign build time default values. */
    cfg.host = strdup_s( DEF_HOST );
    cfg.port = strdup_s( DEF_PORT );
    cfg.select_timeout = SELECT_TIMEOUT_S;
    cfg.msg_timeout = MSG_TIMEOUT_S;
    cfg.resp_timeout = RESP_TIMEOUT_S;
    cfg.offer_timeout = OFFER_TIMEOUT_S;
    cfg.username = strdup( DEF_USER );
    cfg.pubkey = strdup( DEF_PUBKEY );
    cfg.privkey = strdup( DEF_PRIVKEY );
    cfg.autoconnect = 0;
    cfg.no_clobber = 0;
    cfg.st = CLT_INVALID;
    for ( int i = 0; i < 3; ++i )
        cfg.istty[i] = isatty( i );

    /* Parse default configuration file. */
    tmp = getenv( "HOME" );
    cfg_filename = strdupcat2_s( tmp ? tmp : ".", "/", CONFIG_PATH );
    errno = 0;
    if ( 0 != ( r = cfg_parse_file( cfg_filename, cfgdef ) ) && 0 != errno )
        DLOG( "fopen(%s) failed: %m\n", cfg_filename );
    free( cfg_filename );

    /* Parse command line options. */
    eval_cmdline( argc, argv );

    /* Last resort to pre-set a sensible user name. */
    if ( ( NULL == cfg.username || '\0' == *cfg.username )
        && NULL != ( tmp = getenv( "USER" ) ) )
    {
        free( cfg.username );
        cfg.username = strdup_s( tmp );
    }
    return r;
}

/**********************************************
 * CONSOLE STUFF
 *
 */

#define PFX_CONN    "CONN"  // connect notification
#define PFX_DISC    "DISC"  // disconnect notification
#define PFX_AUTH    "AUTH"  // fully logged in
#define PFX_NAUT    "NAUT"  // logged out

#define PFX_TLST    "TLST"  // transfer list item
#define PFX_PLST    "PLST"  // peer list item
#define PFX_COUT    "COUT"  // external command output
#define PFX_WDIR    "WDIR"  // current directory info

#define PFX_IMSG    "IMSG"  // informative message
#define PFX_SMSG    "SMSG"  // server message

#define PFX_QPNG    "QPNG"  // received PING request
#define PFX_RPNG    "RPNG"  // received PING response

#define PFX_OFFR    "OFFR"  // received OFFER request
#define PFX_DSTA    "DSTA"  // download started
#define PFX_DFIN    "DFIN"  // download finished
#define PFX_DERR    "DERR"  // download finished
#define PFX_UFIN    "UFIN"  // upload finished

#define PFX_CERR    "CERR"  // command error
#define PFX_LERR    "LERR"  // local error
#define PFX_SERR    "SERR"  // remote error

static void prompt( int on )
{
    if ( cfg.istty[STDIN_FILENO] )
        fprintf( stderr, on ? "\rfrelay> " :"\r       \r" );
}

/*
 * Just to have a central function to control console output.
 */
static int printcon( const char *pfx, const char *fmt, ... )
{
    int r = 0;
    va_list arglist;
    FILE *ofp = stdout;

    prompt( 0 );
    if ( !cfg.istty[STDOUT_FILENO] )
        r += fprintf( ofp, "%s:%s", pfx, fmt ? "" : "\n" );
    if ( NULL != fmt )
    {
        va_start( arglist, fmt );
        r += vfprintf( ofp, fmt, arglist );
        va_end( arglist );
        fflush( ofp );
#ifdef DEBUG
        va_start( arglist, fmt );
        XLOGV( LOG_DEBUG, fmt, arglist );
        va_end( arglist );
#endif
    }
    prompt( 1 );
    return r;
}

static int tlist_cb( transfer_t *t )
{
    static char buf[PATH_MAX];

    if ( !cfg.istty[STDOUT_FILENO] )
        transfer_itostr( buf, sizeof buf, "%i '%n' %o/%s %t %d %a", t );
    else
        transfer_itostr( buf, sizeof buf, "%i '%n' %O/%S %T %D %A", t );
    return printcon( PFX_TLST, "%s\n", buf );
}

static int pcmd_cb( const char *s )
{
    return printcon( PFX_COUT, "%s", s );
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
        DLOG( "getaddrinfo(%s,%s) failed: %s\n", host, service, emsg );
        printcon( PFX_CERR, "%s:%s: %s\n", host, service, emsg );
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
        printcon( PFX_CERR, "%s:%s: %s\n", host, service, strerror( errno ) );
        return -1;
    }
    DLOG( "Connected to [%s:%s].\n", host, service );
    printcon( PFX_CONN, "Connected to %s:%s\n", host, service );
    if ( 0 != set_nonblocking( fd ) )
        DLOG( "set_nonblocking() failed: %m.\n" );
    if ( 0 != set_cloexec( fd ) )
        DLOG( "set_cloexec() failed: %m.\n" );
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
        if ( (time_t)ntime_to_s( HDR_GET_TS(q) ) + cfg.resp_timeout < now )
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
            DLOG( "read() failed: %m.\n" );
        return -1;
    }
    else if ( 0 == r )
    {   /* EOF on stdin. */
        return -2;
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
    if ( NULL == arg[0] || '\0' == arg[0][0] )
    {
        prompt( 1 );
        return -1;
    }
    aline[ arg[a-1] - arg[0] + strlen( arg[a-1] ) ] = '\0';

    enum cmd_enum {
        CMD_NONE = 0,
        CMD_ACCEPT,
        CMD_CD,
        CMD_CMD,
        CMD_CONNECT,
        CMD_DISCONNECT,
        CMD_DROP,
        CMD_EXIT,
        CMD_HELP,
        CMD_LIST,
        CMD_LOGIN,
        CMD_LOGOUT,
        CMD_OFFER,
        CMD_PEERLIST,
        CMD_PING,
        CMD_PWD,
        CMD_REGISTER,
        CMD_REMOVE,
        CMD_SH,
    };
    static struct {
        const char *cmd_name;
        enum cmd_enum cmd;
        const char *help_txt;
    } cmd_list[] = {
        { "?",          CMD_HELP,       "\t\t\tsame as 'help'" },
        { "!",          CMD_SH,         "\t\t\tsame as 'sh'" },
        { "^",          CMD_CMD,        "\t\t\tsame as 'cmd" },
        { "accept",     CMD_ACCEPT,     " offer_id\t\taccept an offer" },
        { "bye",        CMD_EXIT,       "\t\t\tsame as 'exit'" },
        { "cd",         CMD_CD,         " [path]\t\tchange current working directory" },
        { "close",      CMD_DISCONNECT, "\t\t\tclose connection to server" },
        { "cmd",        CMD_CMD,        "prog [args]\t\texecute external program" },
        { "connect",    CMD_CONNECT,    " [host [port]]\tconnect to specified server" },
        { "delete",     CMD_REMOVE,     " D|O,peer,offer\tcancel a pending offer; *,*,* cancels all" },
        { "disconnect", CMD_DISCONNECT, "\t\tsame as 'close'" },
        { "drop",       CMD_DROP,       "\t\t\tdrop account registration" },
        { "exit",       CMD_EXIT,       "\t\t\tterminate frelay" },
        { "help",       CMD_HELP,       "\t\t\tdisplay this command list" },
        { "lcd",        CMD_CD,         "\t\t\tsame as 'cd'" },
        { "list",       CMD_LIST,       "\t\t\tlist active transfers / open offers" },
        { "login",      CMD_LOGIN,      " [user [passwd]]\tlog on to connected server" },
        { "logout",     CMD_LOGOUT,     "\t\t\tlog off from connected server" },
        { "offer",      CMD_OFFER,      " peer_id filename\tplace an offer" },
        { "open",       CMD_CONNECT,    "\t\t\tsame as 'connect'" },
        { "peerlist",   CMD_PEERLIST,   "\t\tget list of peers active on server" },
        { "ping",       CMD_PING,       " [peer_id [text]]\tping server or peer" },
        { "pwd",        CMD_PWD,        "\t\t\tprint current working directory" },
        { "quit",       CMD_EXIT,       "\t\t\tsame as 'exit'" },
        { "register",   CMD_REGISTER,   " password\tcreate or change an account" },
        { "remove",     CMD_REMOVE,     "\t\t\tsame as 'delete'" },
        { "sh",         CMD_SH,         "\t\t\topen an interactive sub-shell" },
        { "who",        CMD_PEERLIST,   "\t\t\tsame as 'peerlist'" },
        { NULL, CMD_NONE, NULL }
    };
    enum cmd_enum cmd = CMD_NONE;
    int cmd_len = strlen( arg[0] );
    for ( int i = 0; NULL != cmd_list[i].cmd_name; ++i )
    {
        if ( 0 != strnicmp( cmd_list[i].cmd_name, arg[0], cmd_len ) )
            continue;
        if ( CMD_NONE != cmd )
        {
            printcon( PFX_CERR, "Ambiguous command\n" );
            return -1;
        }
        cmd = cmd_list[i].cmd;
    }
    r = 0;  // -1: error, 0: remote, 1: local
    switch ( cmd )
    {
    case CMD_HELP:      /* help */
        printcon( PFX_IMSG, "Commands may be abbreviated.  Commands are:\n" );
        for ( int i = 0; NULL != cmd_list[i].cmd_name; ++i )
            printcon( PFX_IMSG, "%s%s\n", cmd_list[i].cmd_name, cmd_list[i].help_txt );
        r = 1;
        break;
    case CMD_PING:      /* ping [destination [notice]] */
        mbuf_compose( &mp, MSG_TYPE_PING_REQ, 0,
                ( 1 < a ) ? strtoull( arg[1], NULL, 16 ) : 0, prng_random() );
        if ( 2 < a )
        {
            cp = aline + ( arg[2] - arg[0] );
            mbuf_addattrib( &mp, MSG_ATTR_NOTICE, strlen( cp ) + 1, cp );
        }
        break;
    case CMD_PEERLIST:  /* peerlist */
        mbuf_compose( &mp, MSG_TYPE_PEERLIST_REQ, 0, 0, prng_random() );
        break;
    case CMD_OFFER:     /* offer destination file [notice] */
        if ( 3 > a )
        {
            printcon( PFX_CERR, "Usage: offer destination file\n" );
            r = -1;
        }
        else
        {
            const transfer_t *o;
            char *bname, *fname;
            if ( CLT_AUTH_OK != cfg.st )
            {   /* Avoid creating bogus entries in transfer list! */
                printcon( PFX_CERR, "Not logged in\n" );
                r = -1;
                break;
            }
            if ( NULL == ( o = offer_new( strtoull( arg[1], NULL, 16 ), arg[2] ) ) )
            {
                printcon( PFX_CERR, "No such file: '%s'\n", arg[2] );
                r = -1;
                break;
            }
            mbuf_compose( &mp, MSG_TYPE_OFFER_REQ, 0, o->rid, prng_random() );
            mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, o->oid );
            fname = strdup_s( arg[2] );
            bname = basename( fname );
            mbuf_addattrib( &mp, MSG_ATTR_FILENAME, strlen( bname ) + 1, bname );
            free( fname );
            mbuf_addattrib( &mp, MSG_ATTR_SIZE, 8, o->size );
        }
        break;
    case CMD_ACCEPT:    /* accept offer_id */
        if ( 2 > a )
        {
            printcon( PFX_CERR, "Usage: accept offer_id\n" );
            r = -1;
        }
        else
        {
            uint64_t rid, oid;
            transfer_t *d;

            if ( 2 > transfer_strtoi( arg[1], NULL, &rid, &oid )
                || NULL == ( d = transfer_match( TTYPE_DOWNLOAD, rid, oid ) ) )
            {
                printcon( PFX_CERR, "Invalid offer ID %"PRIx64"\n", oid );
                r = -1;
                break;
            }
            if ( 0 == download_resume( d ) )
            {
                if ( 0 < d->offset )
                {
                    transfer_itostr( aline, sizeof aline, "%i '%n' %O/%S", d );
                    printcon( PFX_DSTA, "%s download resumed\n", aline );
                }
                else
                {
                    transfer_itostr( aline, sizeof aline, "%i '%n' %S", d );
                    printcon( PFX_DSTA, "%s download started\n", aline );
                }
            }
            else if ( 0 == download_write( d, NULL, 0 ) )
            {
                transfer_itostr( aline, sizeof aline, "%i '%n' %S", d );
                printcon( PFX_DFIN, "%s download started\n", aline );
            }
            else
            {
                transfer_itostr( aline, sizeof aline, "%i '%n'", d );
                printcon( PFX_DERR, "%s download failed: %s\n", aline, strerror( errno ) );
                r = -1;
                break;
            }
            mbuf_compose( &mp, MSG_TYPE_GETFILE_REQ, 0, d->rid, prng_random() );
            mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, oid );
            mbuf_addattrib( &mp, MSG_ATTR_OFFSET, 8, d->offset );
            mbuf_addattrib( &mp, MSG_ATTR_SIZE, 8,
                            d->size < MAX_DATA_SIZE ? d->size : MAX_DATA_SIZE );
        }
        break;
    case CMD_CONNECT:   /* connect [host [port]] */
        if ( 2 > a && NULL == ( arg[1] = cfg.host ) )
        {
            printcon( PFX_CERR, "Usage: connect [host [port]]\n" );
            r = -1;
        }
        else
        {
            if ( 3 > a )
                arg[2] = cfg.port;
            connect_srv( srvfd, arg[1], arg[2] );
            if ( 0 > *srvfd )
            {
                cfg.st = CLT_INVALID;
                r = -1;
            }
            else
            {
                cfg.st = CLT_PRE_LOGIN;
                r = 1;
            }
        }
        break;
    case CMD_DISCONNECT:    /* disconnect */
        if ( 0 <= *srvfd )
        {
            transfer_closeall();
            DLOG( "Closing connection.\n" );
            disconnect_srv( srvfd );
            printcon( PFX_DISC, "Connection closed\n" );
        }
        cfg.st = CLT_INVALID;
        r = 1;
        break;
    case CMD_REGISTER:      /* register user pubkey */
        DLOG( "Registering.\n" );
        if ( 2 > a )
        {
            printcon( PFX_CERR, "Usage: register password\n" );
            r = -1;
        }
        else
        {
            mbuf_compose( &mp, MSG_TYPE_REGISTER_REQ, 0, 0, prng_random() );
            mbuf_addattrib( &mp, MSG_ATTR_PUBKEY, strlen( arg[1] ) + 1, arg[1] );
        }
        break;
    case CMD_DROP:   /* drop */
        DLOG( "Dropping.\n" );
        mbuf_compose( &mp, MSG_TYPE_DROP_REQ, 0, 0, prng_random() );
        break;
    case CMD_LOGIN:     /* login [user] */
        DLOG( "Logging in.\n" );
        if ( 2 > a  && NULL == ( arg[1] = cfg.username ) )
        {
            printcon( PFX_CERR, "Usage: login [user [password]]\n" );
            r = -1;
        }
        else
        {
            if ( 2 < a )
            {
                free( cfg.pubkey );
                cfg.pubkey = strdup_s( arg[2] );
            }
            mbuf_compose( &mp, MSG_TYPE_LOGIN_REQ, 0, 0, prng_random() );
            mbuf_addattrib( &mp, MSG_ATTR_USERNAME, strlen( arg[1] ) + 1, arg[1] );
        }
        break;
    case CMD_LOGOUT:    /* logout */
        DLOG( "Logging out.\n" );
        mbuf_compose( &mp, MSG_TYPE_LOGOUT_REQ, 0, 0, prng_random() );
        break;
    case CMD_LIST:      /* list */
        printcon( PFX_TLST, NULL );
        transfer_list( tlist_cb );
        r = 1;
        break;
    case CMD_REMOVE:    /* remove "T|peer|offer" */
        if ( 2 > a )
        {
            printcon( PFX_CERR, "Usage: remove type|remote_id|offer_id\n" );
            r = -1;
        }
        else
        {
            int n;
            if ( 0 < ( n = transfer_remove( arg[1] ) ) )
            {
                printcon( PFX_IMSG, "Removed %d transfer%s\n", n, 1 != n ? "s" : "" );
                r = 1;
            }
            else
            {
                printcon( PFX_CERR, "Invalid remove mask: '%s'\n", arg[1] );
                r = -1;
            }
        }
        break;
    case CMD_EXIT:  /* exit */
        disconnect_srv( srvfd );
        cfg.st = CLT_INVALID;
        r = -2;
        break;
    case CMD_CD:    /* cd [path] */
        if ( 1 < a )
        {
            cp = aline + ( arg[1] - arg[0] );
            errno = 0;
            if ( 0 != chdir( cp ) )
                printcon( PFX_CERR, "%s\n", strerror( errno ) );
        }
        /* fall- through to CMD_PWD */
    case CMD_PWD:   /* pwd */
        if ( !cfg.istty[STDOUT_FILENO] )
            printcon( PFX_WDIR, "%s\n", getcwd( line, sizeof line ) );
        else
            printcon( PFX_WDIR, "Local directory now %s\n", getcwd( line, sizeof line ) );
        r = 1;
        break;
    case CMD_CMD:   /* cmd prog [args] */
        if ( 2 > a )
        {
            printcon( PFX_CERR, "Usage: cmd \"command\"\n" );
            r = -1;
        }
        else
        {
            cp = aline + ( arg[1] - arg[0] );
            pcmd( cp, pcmd_cb );
            r = 1;
        }
        break;
    case CMD_SH:    /* sh */
        if ( cfg.istty[STDIN_FILENO] && cfg.istty[STDOUT_FILENO] )
        {
            pid_t pid;
            if ( 0 == ( pid = fork() ) )
            {   /* Try various incantations to spawn a shell,
                   until one succeeds or we finally give up. */
                const char *sh[2];
                sh[0] = getenv( "SHELL" );
                sh[1] = DEFAULT_SHELL;
                for ( int i = 0; i < 2; ++i )
                {
                    if ( NULL == sh[i] )
                        continue;
                    DLOG( "Trying %s -l\n", sh[i] );
                    execlp( sh[i], sh[i], "-l", NULL );
                    DLOG( "Trying %s [-]\n", sh[i] );
                    execlp( sh[i], "-", NULL );
                    DLOG( "Trying %s -i\n", sh[i] );
                    execlp( sh[i], sh[i], "-i", NULL );
                    DLOG( "Trying %s\n", sh[i] );
                    execlp( sh[i], sh[i], NULL );
                }
                XLOG( LOG_WARNING, "execlp() failed: %m\n" );
                exit( EXIT_FAILURE );
            }
            else if ( 0 < pid )
            {
                waitpid( pid, NULL, 0 );
                r = 1;
            }
            else
            {
                XLOG( LOG_WARNING, "fork() failed: %m\n" );
                r = -1;
            }
        }
        else
            printcon( PFX_CERR, "Spawning a subshell is only supported in TTY mode.\n" );
        break;
    default:
        printcon( PFX_CERR, "Invalid command. Enter 'help' to get a list of valid commands.\n" );
        DLOG( "unknown command sequence: \"%s\"\n", aline );
        r = -1;
        break;
    }

    if ( 0 == r )
    {
        if ( 0 > *srvfd )
        {
            printcon( PFX_CERR, "Not connected\n" );
            mbuf_free( &mp );
        }
        else if ( NULL != mp )
            enqueue_msg( mp );
    }
    prompt( 1 );
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
    static char buf[PATH_MAX];

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
            printcon( PFX_QPNG, "Ping from %016"PRIx64"%s%s%s\n",
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
                    make_filenames( d, av, cfg.no_clobber );
                    break;
                case MSG_ATTR_SIZE:
                    d->size = NTOH64( *(uint64_t *)av );
                    break;
                default:
                    break;
                }
            }
            if ( NULL == d->name || 0 == strlen( d->name ) || 0 == d->size )
            {
                transfer_invalidate( d );
                status = SC_BAD_REQUEST;
                break;
            }
            transfer_itostr( buf, sizeof buf, "%i '%n' %S", d );
            printcon( PFX_OFFR, "%s offer received\n", buf );
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

            if ( NULL == ( o = transfer_match( TTYPE_OFFER, srcid, oid ) ) )
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
                transfer_itostr( buf, sizeof buf, "%i '%n' %S %D", o );
                printcon( PFX_UFIN, "%s upload finished\n", buf );
                transfer_invalidate( o );
            }
            else if ( NULL == ( data = offer_read( o, offset, &size ) ) )
            {
                status = SC_RANGE_NOT_SATISFIABLE;
                break;
            }
            o->offset = offset + size;
            mbuf_compose( &mp, MSG_TYPE_GETFILE_RES, 0, srcid, trfid );
            mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, oid );
            mbuf_addattrib( &mp, MSG_ATTR_DATA, size, data );
            free( data );
            if ( 0 < size )
                DLOG( "Sending %"PRIu64" bytes of %016"PRIx64" '%s' %3"PRIu64"%% (%"PRIu64"/%"PRIu64")\n",
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
            printcon( PFX_RPNG, "Ping response from %016"PRIx64": RTT=%"PRI_ntime".%03"PRI_ntime"ms\n",
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
            if ( NULL == ( o = transfer_match( TTYPE_OFFER, srcid, oid ) ) )
            {
                DLOG( "Peer %016"PRIx64" referenced invalid offer %016"PRIx64"\n", srcid, oid );
                break;
            }
            DLOG( "Peer %016"PRIx64" received offer %016"PRIx64" '%s'\n", srcid, o->oid, o->name );
        }
        break;
    case MSG_TYPE_GETFILE_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OFFERID == at )
        {
            uint64_t oid = NTOH64( *(uint64_t *)av );
            transfer_t *d = transfer_match( TTYPE_DOWNLOAD, srcid, oid );
            if ( 0 != mbuf_getnextattrib( *pp, &at, &al, &av )
                || MSG_ATTR_DATA != at
                || NULL == d )
            {
                DLOG( "Broken GETFILE response!\n" );
            }
            else
            {
                transfer_itostr( buf, sizeof buf, "%i '%n' %O/%S %D", d );
                if ( 0 == al )
                {
                    if ( d->offset == d->size )
                        printcon( PFX_DFIN, "%s download finished\n", buf );
                    else
                        printcon( PFX_DERR, "%s premature EOF from peer\n", buf );
                    transfer_invalidate( d );
                    if ( 0 != rename( d->partname, d->name ) )
                        printcon( PFX_DERR, "%s Moving '%s' to '%s' failed: %s\n",
                                    buf, d->partname, d->name, strerror( errno ) );
                }
                else if ( 0 != download_write( d, av, al ) )
                {
                    transfer_invalidate( d );
                    printcon( PFX_DERR, "%s write failed, aborted\n", buf );
                }
                else
                {
                    d->offset += al;
                    uint64_t sz = d->size - d->offset;
                    mbuf_compose( &mp, MSG_TYPE_GETFILE_REQ, 0, d->rid, prng_random() );
                    mbuf_addattrib( &mp, MSG_ATTR_OFFERID, 8, oid );
                    mbuf_addattrib( &mp, MSG_ATTR_OFFSET, 8, d->offset );
                    mbuf_addattrib( &mp, MSG_ATTR_SIZE, 8, sz < MAX_DATA_SIZE ? sz : MAX_DATA_SIZE );
                    DLOG( "Received %zu bytes of %016"PRIx64" '%s' %3"PRIu64"%% (%"PRIu64"/%"PRIu64")\n",
                                al, d->oid, d->name, d->offset*100/d->size, d->offset, d->size );
                }
            }
        }
        break;
    case MSG_TYPE_PEERLIST_RES:
        if ( CLT_AUTH_OK == cfg.st && 0ULL == srcid )
        {
            printcon( PFX_PLST, NULL );
            while ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
                && MSG_ATTR_PEERID == at
                && 0 == mbuf_getnextattrib( *pp, &at2, &al2, &av2 )
                && MSG_ATTR_PEERNAME == at2 )
            {
                printcon( PFX_PLST, "%016"PRIx64"%s %s\n", NTOH64( *(uint64_t *)av ),
                        (HDR_GET_DSTID(*pp)==NTOH64( *(uint64_t *)av ))?"*":" ",  (char *)av2  );
            }
        }
        break;
    case MSG_TYPE_REGISTER_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( PFX_IMSG, "Registered\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( PFX_SMSG, "%s\n", (char *)av );
        }
        break;
    case MSG_TYPE_DROP_RES:
        if ( CLT_AUTH_OK == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( PFX_IMSG, "Dropped.\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( PFX_SMSG, "%s\n", (char *)av );
        }
        break;
    case MSG_TYPE_LOGIN_RES:
        if ( CLT_PRE_LOGIN == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && ( MSG_ATTR_OK == at || MSG_ATTR_CHALLENGE == at ) )
        {
            printcon( PFX_IMSG, "Login Ok\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at2, &al2, &av2 ) && MSG_ATTR_NOTICE == at2 )
                printcon( PFX_SMSG, "%s\n", (char *)av2 );
            if ( MSG_ATTR_OK == at )
            {   /* Unregistered user. */
                cfg.st = CLT_AUTH_OK;
                printcon( PFX_AUTH, "No authentication required\n" );
            }
            else if ( MSG_ATTR_CHALLENGE == at )
            {   /* Registered user: authenticate. */
                /* PROOF OF CONCEPT ONLY (road works ahead): */
                if ( 0 == strncmp( av, AUTH_KEY_PLAINTEXT, strlen( AUTH_KEY_PLAINTEXT ) ) )
                {
                    char *key;
                    printcon( PFX_IMSG, "Authenticating\n" );
                    key = strdupcat_s( AUTH_KEY_PLAINTEXT, cfg.pubkey ? cfg.pubkey : "" );
                    mbuf_compose( &mp, MSG_TYPE_AUTH_REQ, 0, 0, trfid );
                    mbuf_addattrib( &mp, MSG_ATTR_DIGEST, strlen( key ) + 1, key );
                    free( key );
                    cfg.st = CLT_LOGIN_OK;
                }
                else
                    printcon( PFX_SERR, "Authentication method not supported\n" );
            }
        }
        break;
    case MSG_TYPE_AUTH_RES:
        if ( CLT_LOGIN_OK == cfg.st
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( PFX_AUTH, "Authenticated\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( PFX_SMSG, "%s\n", (char *)av );
            cfg.st = CLT_AUTH_OK;
        }
        break;
    case MSG_TYPE_LOGOUT_RES:
        if ( ( CLT_LOGIN_OK == cfg.st || CLT_AUTH_OK == cfg.st )
            && 0ULL == srcid
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_OK == at )
        {
            printcon( PFX_NAUT, "Logged out\n" );
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                printcon( PFX_SMSG, "%s\n", (char *)av );
            cfg.st = CLT_PRE_LOGIN;
        }
        break;

    /* Errors: */
    case MSG_TYPE_AUTH_ERR:
        if ( CLT_LOGIN_OK == cfg.st )
            cfg.st = CLT_PRE_LOGIN;
        /* Fall through to default! */
    default:
        if ( MCLASS_IS_ERR( mtype )
            && 0 == mbuf_getnextattrib( *pp, &at, &al, &av )
            && MSG_ATTR_ERROR == at )
        {
            uint64_t ec = NTOH64( *(uint64_t *)av );
            char *es = NULL;
            if ( 0 == mbuf_getnextattrib( *pp, &at, &al, &av ) && MSG_ATTR_NOTICE == at )
                es = av;
            printcon( PFX_SERR, "%s Error %"PRIu64"%s%s.\n",
                        mtype2str( mtype ), ec, es?" ":"", es?es:"" );
        }
        else
        {
            printcon( PFX_LERR, "Unhandled message 0x%04"PRIx16" (%s_%s)!\n",
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
    printcon( PFX_DISC, "Connection to remote host failed\n" );
    return nset;
}

int main( int argc, char *argv[] )
{
    int srvfd = -1;
    struct timeval to_sav;

    /* Initialization. */
    XLOG_INIT( argv[0], LOG_TO_FILE, stderr );
    signal( SIGPIPE, SIG_IGN );     /* Ceci n'est pas une pipe. */
    //signal( SIGINT, SIG_IGN );      /* Ctrl-C no more. */
    /* TODO: ignore or handle other signals? */
    prng_srandom( ntime_get() ^ getpid() );
    init_config( argc, argv );
    to_sav = (struct timeval){ .tv_sec = cfg.select_timeout, 0 };

    if ( cfg.autoconnect && NULL != cfg.host && '\0' != *cfg.host )
    {
        connect_srv( &srvfd, cfg.host, cfg.port );
        if ( 0 <= srvfd )
            cfg.st = CLT_PRE_LOGIN;
    }
    DLOG( "Entering main loop.\n" );
    prompt( 1 );
    while ( 1 )
    {
        int nset;
        int maxfd;
        fd_set rfds, wfds;
        struct timeval to;
        static time_t last_upkeep = 0;
        time_t now;

        now = time( NULL );
        if ( now - last_upkeep > to_sav.tv_sec )
        {   /* Avoid doing upkeep continuously under load. */
            last_upkeep = now;
            if ( CLT_AUTH_OK == cfg.st )
            {   /* Send ping to server. */
                mbuf_t *mb = NULL;
                mbuf_compose( &mb, MSG_TYPE_PING_IND, 0ULL, 0ULL, prng_random() );
                enqueue_msg( mb );
            }
            upkeep_pending();
            transfer_upkeep( cfg.offer_timeout );
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
        to = to_sav;
        nset = select( maxfd + 1, &rfds, &wfds, NULL, &to );
        if ( 0 < nset )
        {
            nset = handle_srvio( nset, &srvfd, &rfds, &wfds );
            if ( 0 < nset && FD_ISSET( STDIN_FILENO, &rfds ) )
            {
                --nset;
                if ( -2 == process_stdin( &srvfd ) )
                    break;  /* 'exit' or EOF on stdin terminates client. */
            }
            if ( 0 < nset ) /* Can happen on read errors. */
                DLOG( "%d file descriptors still set!\n", nset );
        }
        else if ( 0 == nset )
        {
            /* Select timed out */
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
    transfer_closeall();
    disconnect_srv( &srvfd );
    exit( EXIT_SUCCESS );
}

/* EOF */
