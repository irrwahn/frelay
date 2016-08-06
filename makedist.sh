#!/bin/sh

#TODOs
# 1. versioning
# 2.  stuff


VER=r$(svnversion)

tar cvzf frelay-$VER.tar.gz -T dist.lst
