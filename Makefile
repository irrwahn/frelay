##
## Makefile for project: frelay
##
## Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer
##   in the documentation and/or other materials provided with the
##   distribution.
## * Neither the name of the copyright holder nor the names of its
##   contributors may be used to endorse or promote products derived
##   from this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
##
##


PROJECT := frelay

export CC      := gcc
export CFLAGS  := -std=c99 -pedantic -Wall -Wextra -O2 -I./lib -MMD -MP
ifeq    ($(DEBUG),1)
	CFLAGS  += -g3 -pg -DDEBUG
else
	CFLAGS  += -s
## TODO: replace -s compiler option with explicitly calling strip
endif

LD      := $(CC)
LIBDIR  := ./lib
LIBS    := -lfrutil
LDFLAGS := -L$(LIBDIR)

RM      := rm -f

COMSRC  := message.c statcodes.c util.c

SRVBIN  := $(PROJECT)srv
SRVSRC  := $(COMSRC) srvmain.c srvuserdb.c
SRVOBJ  := $(SRVSRC:%.c=%.o)
SRVDEP  := $(SRVOBJ:%.o=%.d)

CLTBIN  := $(PROJECT)clt
CLTSRC  := $(COMSRC) cltmain.c
CLTOBJ  := $(CLTSRC:%.c=%.o)
CLTDEP  := $(CLTOBJ:%.o=%.d)

SELF    := $(lastword $(MAKEFILE_LIST))


.PHONY: all lib clean distclean

all: $(SRVBIN) $(CLTBIN)

$(SRVBIN): $(SRVOBJ) $(SELF) lib
	$(LD) $(LDFLAGS) $(SRVOBJ) $(LIBS) -o $(SRVBIN)

$(CLTBIN): $(CLTOBJ) $(SELF) lib
	$(LD) $(LDFLAGS) $(CLTOBJ) $(LIBS) -o $(CLTBIN)

$(SRVOBJ): srvcfg.h

$(CLTOBJ): cltcfg.h

lib:
	$(MAKE) -C $(LIBDIR)

%.o: %.c $(SELF) config.h
	$(CC) -c $(CFLAGS) -o $*.o $*.c

srvcfg.h: srvcfg.def.h
	cp $< $@

cltcfg.h: cltcfg.def.h
	cp $< $@

clean:
	$(MAKE) -C $(LIBDIR) $@
	$(RM) $(SRVBIN) $(CLTBIN) $(SRVOBJ) $(CLTOBJ) $(DEP)

distclean: clean
	$(MAKE) -C $(LIBDIR) $@
	$(RM) srvcfg.h cltcfg.h *.d

-include $(DEP)

## EOF
