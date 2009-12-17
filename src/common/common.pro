#-*- mode: makefile; tab-width: 4; -*-
#
# This library provides basic initialization function used in all executables,
# including the GUI and all compilers
# on QT

include(../../qmake.inc)

TEMPLATE = lib

SOURCES = init.cpp

HEADERS = ../../config.h commoninit.h

CONFIG += staticlib

TARGET = common

INSTALLS -= target

exists(qmake.inc):include( qmake.inc)
