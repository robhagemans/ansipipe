all: launcher example-c example-cpp example-single example-gui

launcher: launcher.c
	gcc launcher.c -o launcher

example-c: example.c ansipipe.h launcher
	gcc example.c -o example-c
ifeq ($(OS),Windows_NT)
	cp launcher.exe example-c.com
endif
    
example-cpp: example.cpp ansipipe.hpp launcher
	g++ example.cpp -o example-cpp
ifeq ($(OS),Windows_NT)
	cp launcher.exe example-cpp.com
endif

example-single: example-single.c launcher.c ansipipe.h
	gcc example-single.c launcher.c -o example-single -D ANSIPIPE_SINGLE    

example-gui: example-gui.cpp ansipipe.hpp launcher
ifeq ($(OS),Windows_NT)
	g++ example-gui.cpp -o example-gui -fpermissive -mwindows
	cp launcher.exe example-gui.com
else
	touch example-gui
endif

