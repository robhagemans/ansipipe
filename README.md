ANSI|pipe
========

**ANSI|pipe** is a small utility that provides no-hassle support for ANSI 
escape sequences and UTF-8 on Windows. It addresses three issues I encountered 
when porting Unix console applications to Windows:  

1.  Windows executables must be compiled as either a 'console' or a 'GUI'
    application. Thus, windowed applications either have an ugly console window 
    pop up or do not have access to the console at all — even if started from 
    the command line.  
2.  The Windows command line does not recognise ANSI escape sequences. Any code
    that relies on escape sequences to prettify the console interface would need
    to be rewritten using OS-specific toolkits.  
3.  Under modern Unix it is a safe bet that the console will understand 
    UTF-8, but in the Windows world UTF-16 is dominant. Even worse, the standard 
    Windows console still uses legacy codepages rather than Unicode by default.  
    
If your goal is just to maintain a Windows port of a utility you've written
originally for the Unix world, digging into the Windows API is an unwelcome 
distraction.  

**ANSI|pipe** is a less-than-50KiB executable that solves all three issues in one go.


## How to use it

It's as simple as:  

    #include <iostream>
    #include "ansipipe.h"
    
    int main() 
    {
        ansipipe_init();
        
        std::cout << "\x1b]2;ANSI|pipe demo\x07"
            << "\n\n\n\x1b[2AHello,\x1b[1A\x1b[91m World!\x1b[2B\x1b[0m "
            << "Здравствуй,\x1b[1A\x1b[92m мир!\x1b[0m\x1b[2B "
            << "Γεια σου\x1b[1A\x1b[94m κόσμε!\x1b[2B\x1b[0m\n";
        std::cout << "Type something: ";
    
        std::string input;
        std::getline(std::cin, input);
        std::cout << "You typed \x1b[45;1m" << input << "\x1b[0m.\n";
    
        return 0;
    }

The C header is self-contained and MIT-licensed, so there's no licence worries 
when linking it to your project. Apart from the C header, a Python module is also provided. 

There are two ways to deploy it. One way:  

    E:\ANSIpipe> gcc launcher.c -o launcher.exe
    E:\ANSIpipe> g++ example.cpp -o example.exe
    E:\ANSIpipe> launcher example
    
    
The other way:  

    E:\ANSIpipe> gcc launcher.c -o example.com
    E:\ANSIpipe> g++ example.cpp -o example.exe
    E:\ANSIpipe> example

Either way, any remaining command-line arguments are sent to your app's executable.  
A Python app that uses `import ansipipe` can be run with:

    launcher python example.py

And this is what it looks like:

![Screenshot of ANSI|pipe in action](/../screenshots/screenshot.png?raw=true)

Be sure to enable a Unicode-aware terminal font, such as **Lucida Console** 
which is available on Windows by default. (_Unfortunately it has no support 
for East or South Asian scripts. A good free monospace font with support for 
many Western and East Asian scripts is **WenQuanYi Zen Hei Mono**_.)

## Single-binary mode

If you're creating a pure console application, issue (1) is not a concern. You 
simply want no-hassle support for ANSI escape sequences and UTF-8 on Windows.
In this case, if your app is written in C or C++, you can link ANSI|pipe into 
your binary and it all works automatically. It's just as easy:

    #include <stdio.h>
    #include <string.h>
    #include "ansipipe.h"
    
    int main(int argc, char *argv[]) 
    {
        ANSIPIPE_LAUNCH(argc, argv);
    
        printf("\x1b]2;%s\x07", "ANSI|pipe demo");
        printf("\n\n\n\x1b[2AHello,\x1b[1A\x1b[91m World!\x1b[2B\x1b[0m ");
        printf("Здравствуй,\x1b[1A\x1b[92m мир!\x1b[0m\x1b[2B "); 
        printf("Γεια σου\x1b[1A\x1b[94m κόσμε!\x1b[2B\x1b[0m\n");
        printf("Type something: ");
    
        char inbuf[1024];
        fgets(inbuf, sizeof(inbuf), stdin);
        if (strlen(inbuf) > 0) inbuf[strlen(inbuf)-1] = 0;
        
        printf("You typed \x1b[45;1m%s\x1b[0m.\n", inbuf);
    
        return 0;
    }

Compile and run:

    E:\ANSIpipe> gcc example-single.c launcher.c -o example-single.exe -D ANSIPIPE_SINGLE
    E:\ANSIpipe> example-single


## How it does it

ANSI|pipe creates a named pipe for each of `stdin`, `stdout` and `stderr`. 
It converts Windows keystrokes into a byte stream of UTF-8 and ANSI escape 
sequences, and sends this into the `stdin` pipe. At the same time, it takes the
input from the `stdout` and `stderr`, interprets and the ANSI sequences and
executes them on the Windows API, converts the text from UTF-8 to UTF-16 and 
prints it on the console.

As for the launcher: the Windows shell gives preference to `.COM` over `.EXE` 
when globbing an executable, so that all command-line access to `MYAPP` will 
naturally be routed through ANSI|pipe if you've named the launcher `MYAPP.COM`. 
Meanwhile, any GUI access through the Start Menu or the File Manager 
can use `MYAPP.EXE` and avoid a console window. If ANSI|pipe sees its name ends 
in `.EXE`, it will take as its first argument the executable to run; if its name
end in `.COM` it will look for the same name with an `.EXE` extension.   

I've compiled ANSI|pipe using MinGW GCC. You may need to modify the code for it
to compile on MS Visual C or LVVM/Clang. 


## Acknowledgements

ANSI|pipe uses parts of two existing projects:  
-   [**Win32::Console::ANSI**](http://search.cpan.org/~jlmorel/Win32-Console-ANSI-1.08/lib/Win32/Console/ANSI.pm),
    a Perl extension by **J-L Morel** which implements ANSI escape 
    sequences on the Windows console.  
-   [**dualsubsystem**](https://code.google.com/p/dualsubsystem/), a utility by 
    **`gaber...@gmail.com`** based on code by **Richard Eperjesi**, which
    sends standard I/O through named pipes to a .COM file that handles it.  

All modifications and additions to this project are by Rob Hagemans. I'm also
responsible for all the bugs in the resulting code. If you find one, please let 
me know.  

## Copyrights

**Win32::Console::ANSI**, copyright (c) 2003-2014 J-L Morel.  
**dualsubsystem**, copyright (c) `gaber...@gmail.com` and Richard Eperjesi.  
**ANSI|pipe**, copyright (c) 2015 Rob Hagemans.  

## Licence
     
**ANSI|pipe** is free software. The launcher is released under the GNU GPL 
(version [2](http://www.gnu.org/licenses/gpl-2.0.html) 
or [3](http://www.gnu.org/licenses/gpl-3.0.html)) and Perl's 
[Artistic License](http://dev.perl.org/licenses/artistic.html).
The linkable header files and the console examples are released under the [MIT license](http://opensource.org/licenses/MIT).
The GUI example is released under the [Microsoft Limited Public License](https://msdn.microsoft.com/en-us/cc300389.aspx).
    
    


