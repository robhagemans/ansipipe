all: launcher example-c example-cpp example-single example-gui

ifeq ($(OS),Windows_NT)
launcher: launcher.c
	gcc launcher.c -o launcher

example-c: example.c ansipipe.h launcher
	gcc example.c -o example-c
	cp launcher.exe example-c.com
    
example-cpp: example.cpp ansipipe.h launcher
	g++ example.cpp -o example-cpp
	cp launcher.exe example-cpp.com

example-single: example-single.c launcher.c ansipipe.h
	gcc example-single.c launcher.c -o example-single -D ANSIPIPE_SINGLE    

example-gui: example-gui.cpp ansipipe.h launcher
	g++ example-gui.cpp -o example-gui -fpermissive -mwindows
	cp launcher.exe example-gui.com
else
launcher: launcher.c
	gcc launcher.c -o launcher

example-c: example.c ansipipe.h launcher
	gcc example.c -o example-c
    
example-cpp: example.cpp ansipipe.h launcher
	g++ example.cpp -o example-cpp

example-single: example-single.c launcher.c ansipipe.h
	gcc example-single.c launcher.c -o example-single -D ANSIPIPE_SINGLE    

example-gui: example-gui.cpp ansipipe.h launcher
	touch example-gui
endif

