# Makefile for pet application

include /vobs/libs/makefiles/MakeStd.inc

USESOLIBS = true

NAME = pet

PROG1 = $(NAME)
SRCS1 = $(PROG1).cxx
PRIVATE_HEADERS1 = $(PROG1).hxx petMenu.cxx
LIBS1 = pet UIQt utils basics cdevCns name UIUtils gpm

include $(MAKEDIR)/MakeApp.inc
