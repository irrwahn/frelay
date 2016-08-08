/*
 * srvcfg.h
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


#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED


/* Default interface to bind to; empty string means: any. */
#define DEF_INTERFACE   ""

/* Default frelay service port. (Neighbor of the mumbler ^.^) */
#define DEF_PORT        "64740"

/* Maximum number of simultaneously connected clients,
 * typically limited to 1024 on Linux. */
#define MAX_CLIENTS     100

/* Server idle timeout (upkeep interval) in seconds. */
#define SEL_TIMEOUT_S   10

/* Maximum allowed intra-message receive gap in seconds. */
#define MSG_TIMEOUT_S   5

/* Client TCP connection idle timeout in seconds. */
#ifdef DEBUG
#define CONN_TIMEOUT_S  60
#else
#define CONN_TIMEOUT_S  240
#endif

/* Configuration file */
#define CONFIG_PATH     "/etc/frelay.conf"

/* User database file */
#define USERDB_PATH     "/var/lib/frelay/user.db"

/* External command to produce a welcome message. */
#define MOTD_CMD        "echo 'Welcome!'"

/* Registration and logoff text messages. */
#define TXT_REGISTERED  "Account created / modified."
#define TXT_DROPPED     "Account registration dropped."
#define TXT_BYE         "Bye."


#endif /* ndef _H_INCLUDED */

/* EOF */
