# $Id$
#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=my_topoh.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
