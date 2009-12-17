#-*- mode: makefile; tab-width: 4; -*-
#
include(../../qmake.inc)

SOURCES	 =  ipt.cpp

HEADERS	 = ../../config.h

!win32 {
	QMAKE_COPY = ../../install.sh -m 0755 -s
}

win32: CONFIG += console

LIBS += $$LIBS_FWCOMPILER

INCLUDEPATH += ../common ../iptlib ../compiler_lib/
DEPENDPATH   = ../common ../iptlib ../compiler_lib

win32:LIBS  += ../common/release/libcommon.lib ../iptlib/release/iptlib.lib ../compiler_lib/release/compilerdriver.lib 
!win32:LIBS += ../common/libcommon.a ../iptlib/libiptlib.a ../compiler_lib/libcompilerdriver.a

win32:PRE_TARGETDEPS  = ../common/release/libcommon.lib ../iptlib/release/iptlib.lib ../compiler_lib/release/compilerdriver.lib
!win32:PRE_TARGETDEPS = ../common/libcommon.a ../iptlib/libiptlib.a ../compiler_lib/libcompilerdriver.a

TARGET = fwb_ipt


