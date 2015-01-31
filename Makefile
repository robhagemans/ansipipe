all: launcher example-c example-cpp example-single example-gui

launcher: launcher.c
	gcc launcher.c -o launcher

example-c: example.c ansipipe.h
	gcc example.c -o example-c

example-cpp: example.cpp ansipipe.hpp
	g++ example.cpp -o example-cpp

example-single: example-single.c launcher.c ansipipe.h
	gcc example-single.c launcher.c -o example-single -D ANSIPIPE_SINGLE    

example-gui: example-gui.cpp ansipipe.hpp
	g++ example-gui.cpp -o example-gui -fpermissive -mwindows

