# Makefile for pet application
# Define BASEDIR/MAKEDIR environment variables if not already set
ifndef BASEDIR
	BASEDIR := $(PWD)
	MAKEDIR := $(BASEDIR)/libs/makefiles
endif

include ${BASEDIR}/libs/makefiles/MakeStd.inc

NAME = pet

PROG1 = $(NAME)
SRCS1 = $(PROG1).cxx
PRIVATE_HEADERS1 = $(PROG1).hxx petMenu.cxx
LIBS1 = pet agsPage UI UITable utils basics cdevCns name UIUtils
ifdef XRTHOME
LIBS1 += gpm
endif

USESOLIBS = true

include $(MAKEDIR)/MakeApp.inc
