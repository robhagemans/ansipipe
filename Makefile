all: launcher examples

launcher:
	gcc launcher.c -o launcher.exe

examples:
	gcc example.c -o example-c.exe
	g++ example.cpp -o example-cpp.exe
	g++ example-gui.cpp -o example-gui.exe -fpermissive -mwindows
    

