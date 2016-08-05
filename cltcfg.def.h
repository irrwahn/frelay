/*
 * cltcfg.h
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


/* Default frelay server address. */
#define DEF_HOST            "localhost"

/* Default frelay service port. (Neighbor of the mumbler ^.^) */
#define DEF_PORT            "64740"

/* Idle timeout for select(), i.e. maximum interval between upkeeps. */
#define SELECT_TIMEOUT_S    10

/* Maximum intra-message receive gap. */
#define MSG_TIMEOUT_S       5

/* Timespan during which a response to a request is considered valid. */
#define RESP_TIMEOUT_S      30

/* Inactivity timeout for an open offer. */
#define OFFER_TIMEOUT_S     300

/* Default user name and credentials. */
#define DEF_USER            ""
#define DEF_PUBKEY          ""
#define DEF_PRIVKEY         ""

/* Config file location, relative to users home directory.
 * CAVEAT: Leading slash is mandatory! */
#define CONFIG_PATH         "/.config"

/* Argument vector used to start an interactive shell. */
#define DEFAULT_SHELL      (char *[]){ "/bin/sh", "-l", (char *)NULL }


#endif /* ndef _H_INCLUDED */

/* EOF */
