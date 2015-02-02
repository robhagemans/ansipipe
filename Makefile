# This Makefile is targeted at Nmake and MSVCC.
# See GNUmakefile for MINGW/GCC. GCC will use that file by default.

all: launcher example-c example-cpp example-single example-gui

launcher: launcher.c
 cl launcher.c

example-c: example.c ansipipe.h launcher
 cl example.c /Feexample-c.exe
 cp launcher.exe example-c.com
    
example-cpp: example.cpp ansipipe.h launcher
 cl example.cpp /EHsc /Feexample-cpp.exe
 cp launcher.exe example-cpp.com

example-single: example-single.c launcher.c ansipipe.h
 cl example-single.c launcher.c /DANSIPIPE_SINGLE 

example-gui: example-gui.cpp ansipipe.h launcher
 cl example-gui.cpp /EHsc /link /subsystem:windows user32.lib gdi32.lib   
 cp launcher.exe example-gui.com

clean:
 rm *.exe *.com
