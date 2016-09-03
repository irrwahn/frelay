!!! #################################################
!!! # IMPORTANT NOTE: DO NOT EDIT THIS TEMPLATE!    #
!!! # Run `make config´ and edit config.mk instead, #
!!! # or just run `make´ to use the defaults.       #
!!! #################################################

# Adjust to match your standard build system tools:
export CC      ?= cc
export LD      := $(CC)
export STRIP   := strip
export AR      := ar -rs

# Only edit these flags, if you really know what you're doing!
export CFLAGS  := -std=c99 -pedantic -Wall -Wextra -I./lib -MMD -MP
export CRFLAGS := -O2 -DNDEBUG
export CDFLAGS := -O0 -DDEBUG -g3 -pg -ggdb

# Generic tool shorts:
export SH      := sh
export CP      := cp -af
export CPV     := cp -afv
export MV      := mv -f
export RM      := rm -rf
export RMV     := rm -rfv
export MKDIR   := mkdir -p
export TOUCH   := touch
export LN      := ln -sf
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),FreeBSD)
    export TAR := gtar
else
    export TAR := tar
endif

# Default install prefix:
export PREFIX  ?= /usr/local

# EOF
