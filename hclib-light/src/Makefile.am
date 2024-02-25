SUBDIRS =

# Includes folders
INCLUDES_DIR = -I$(top_srcdir)/inc -I$(top_srcdir)/src/inc

# cflags: important to define that otherwise we inherit default values too
CFLAGS = -Wall -g -O3 -std=c11
CXXFLAGS = -Wall -g -O3 -std=c++11
LDFLAGS = -lpthread

# build the hclib library
lib_LTLIBRARIES = libhclib.la

# These are populated in sub-folder's Makefile.inc
noinst_LTLIBRARIES =
libhclib_la_LIBADD =

AM_CXXFLAGS = 

libhclib_la_SOURCES = hclib-runtime.c hclib-deque.c hclib_cpp.cpp

# build hclib
libhclib_la_CXXFLAGS= $(CXXFLAGS) $(INCLUDES_DIR) $(AM_CXXFLAGS)
libhclib_la_CFLAGS= $(CFLAGS) $(INCLUDES_DIR) $(AM_CXXFLAGS)

MAINTAINERCLEANFILES = Makefile.in
