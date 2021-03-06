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

SELF    := $(lastword $(MAKEFILE_LIST))
BLDCFG  := config.mk

export LIBDIR  := ./lib
export LIBS    := -lfrutil -lrt
export LDFLAGS := -L$(LIBDIR)

COMSRC  := auth.c cfgparse.c message.c statcodes.c util.c

SRVBIN  := $(PROJECT)srv
SRVSRC  := $(COMSRC) srvmain.c srvuserdb.c
SRVOBJ  := $(SRVSRC:%.c=%.o)
SRVDEP  := $(SRVOBJ:%.o=%.d)

CLTBIN  := $(PROJECT)clt
CLTSRC  := $(COMSRC) cltmain.c transfer.c
CLTOBJ  := $(CLTSRC:%.c=%.o)
CLTDEP  := $(CLTOBJ:%.o=%.d)

BINDIR = $(PREFIX)/bin
ICONDIR = $(PREFIX)/share/icons/hicolor/scalable/apps
DOCDIR = $(PREFIX)/share/doc/frelay
MANDIR = $(PREFIX)/share/man/man1
EXAMPLEDIR = $(DOCDIR)/examples
APPDIR = $(PREFIX)/share/applications

all: release

release: CFLAGS += $(CRFLAGS)
release: TAG = -rls
release: version lib $(SRVBIN) $(CLTBIN)
	$(STRIP) $(SRVBIN)
	$(STRIP) $(CLTBIN)

debug: CFLAGS += $(CDFLAGS)
debug: TAG = -dbg
debug: version lib $(SRVBIN) $(CLTBIN)

# Server binary:
$(SRVBIN): $(SRVOBJ) $(SELF)
	$(LD) $(LDFLAGS) $(SRVOBJ) $(LIBS) -o $(SRVBIN)

# Client binary:
$(CLTBIN): $(CLTOBJ) $(SELF)
	$(LD) $(LDFLAGS) $(CLTOBJ) $(LIBS) -o $(CLTBIN)

$(SRVOBJ): srvcfg.h

$(CLTOBJ): cltcfg.h

lib:
	$(MAKE) -C $(LIBDIR) $(MAKECMDGOALS)

%.o: %.c $(SELF) config.h
	$(CC) -c $(CFLAGS) -o $*.o $*.c

srvcfg.h: srvcfg.def.h
	@grep -v '^!!!' $< > $@ && echo "Generated $@"

cltcfg.h: cltcfg.def.h
	@grep -v '^!!!' $< > $@ && echo "Generated $@"

clean:
	$(MAKE) -C $(LIBDIR) $@
	$(RM) $(SRVBIN) $(CLTBIN) $(SRVOBJ) $(CLTOBJ) *.d

distclean: clean
	$(MAKE) -C $(LIBDIR) $@
	$(RM) srvcfg.h cltcfg.h version.h $(BLDCFG)

dist: version
	$(eval $@_NAME := $(PROJECT)-$(VERSION))
	$(TAR) --transform "s,^,$($@_NAME)/,S" -cvzf "$($@_NAME).tar.gz" -T dist.lst

install: release
	@echo Installing to $(PREFIX) ...
	@$(MKDIR) $(BINDIR)
	@$(CPV) frelayclt frelaysrv $(BINDIR)
	@$(CPV) pygui/frelay-gui.py $(BINDIR)/frelay-gui
	@$(MKDIR) $(ICONDIR)
	@$(CPV) pygui/icon_src/frelay.svg $(ICONDIR)
	@$(MKDIR) $(DOCDIR)
	@$(CPV) CREDITS $(DOCDIR)
	@$(CPV) README.md $(DOCDIR)
	@$(MKDIR) $(MANDIR)
	$(GZIP_C) doc/frelay-gui.1 > $(MANDIR)/frelay-gui.1.gz
	$(GZIP_C) doc/frelayclt.1 > $(MANDIR)/frelayclt.1.gz
	$(GZIP_C) doc/frelaysrv.1 > $(MANDIR)/frelaysrv.1.gz
	@$(MKDIR) $(EXAMPLEDIR)
	@$(CPV) frelayclt.sample.conf frelaysrv.sample.conf $(EXAMPLEDIR)
	@$(CPV) pygui/frelay-gui.sample.conf pygui/autoaccept.sh pygui/offer.sh $(EXAMPLEDIR)
	@$(MKDIR) $(APPDIR)
	@$(CPV) pygui/frelay-gui.desktop $(APPDIR)

uninstall:
	@echo Uninstalling from $(PREFIX) ...
	-@$(RMV) $(BINDIR)/frelayclt $(BINDIR)/frelaysrv $(BINDIR)/frelay-gui
	-@$(RMV) $(MANDIR)/frelay-gui.1.gz $(MANDIR)/frelayclt.1.gz $(MANDIR)/frelaysrv.1.gz
	-@$(RMV) $(EXAMPLEDIR)
	-@$(RMV) $(DOCDIR)

# Generate user editable config files from template:
config: $(BLDCFG) srvcfg.h cltcfg.h

$(BLDCFG): config.def.mk
	@grep -v '^!!!' $< > $(BLDCFG) && echo "Generated $(BLDCFG)"

# Generate version header:
version:
	$(eval VERSION := $(shell $(SH) version.sh version.in version.h $(TAG)))
	@echo "Version $(VERSION)"

# Include generated files:
-include $(SRVDEP)
-include $(CLTDEP)
-include $(BLDCFG)

.PHONY: all release debug config version lib dist clean distclean install uninstall

## EOF
