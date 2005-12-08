# Makefile for pet application

include /vobs/libs/makefiles/MakeStd.inc

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
