## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)

AM_CCASFLAGS = $(INCLUDES)

noinst_LIBRARIES = lib.a

#SUBDIRS = \
#  $(EXTRA_SUBDIRS)\
#  .
#SUBLIBS = \
#  $(EXTRA_SUBLIBS)

lib_a_SOURCES = getreent.c crt0.c nedmalloc.c \
                mman.c pthread_tls.c mallocr.c incoherent_malloc.c \
                sched.c sig.c unistd_misc.c
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)

ACLOCAL_AMFLAGS = -I ../../.. -I ../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
