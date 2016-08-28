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

# Generic tool shorts:
export SH      := sh
export CP      := cp -dR
export MV      := mv -f
export RM      := rm -f
export MKDIR   := mkdir -p
export TOUCH   := touch
export LN      := ln -s -f
export TAR     := tar

# EOF
