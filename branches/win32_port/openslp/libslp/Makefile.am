#
# Makefile.am
#   Automake File
#     The linked in library libcommonlibslp, which is from ../common
#     containing exacts functions which are required.
#

lib_LTLIBRARIES	= libslp.la
include_HEADERS = slp.h
INCLUDES	= -I$(top_srcdir)/common 

libslp_la_SOURCES = libslp_dereg.c   \
	libslp_findscopes.c \
	libslp_reg.c \
	libslp_findsrvs.c \
	libslp_parse.c \
	libslp_property.c  \
	libslp_handle.c  \
	libslp_thread.c \
	libslp_network.c \
	libslp_findattrs.c \
	libslp_delattrs.c \
	libslp_findsrvtypes.c \
	libslp_knownda.c
libslp_la_LIBADD = ../common/libcommonlibslp.la
libslp_la_LDFLAGS = -version-info 0:1:0
