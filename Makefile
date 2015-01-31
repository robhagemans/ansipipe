all: launcher examples

launcher:
	gcc launcher.c -o launcher.exe

examples:
	gcc example.c -o c-example.exe
	g++ example.cpp -o cpp-example.exe
    

