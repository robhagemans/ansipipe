ANSI|pipe
========

ANSI|pipe is a small utility that addresses three issues I encountered when
porting Unix console applications to Windows:  

1.  Windows executables must be compiled as either a 'console' or a 'GUI'
    application. GUI applications either have an ugly console window pop up or 
    do not have access to the console even if started from the command line.  
2.  The Windows command line does not recognise ANSI escape sequences. Any code
    that relies on escape sequences to prettify the console interface needs to 
    be rewritten using OS-specific toolkits.  
3.  While under modern Unix, it is a safe bet that the console will understand 
    UTF-8, in the Windows world UTF-16 is dominant. Even worse, the standard 
    Windows console still uses legacy codepages rather than Unicode by default.  
    
If your goal is just to maintain a Windows port of a utility you've written
originally for the Unix world, this is an unwelcome distraction. Moreover, 
solving it on an issue-by-issue basis requires digging into Windows API 
functions that are likely to be unfamiliar.  


## How to use it

It's as simple as:  

    #include <iostream>
    #include "ansipipe.hpp"
    
    int main() 
    {
        ansipipe_init();
        
        std::cout << "\x1b]2;ANSI|pipe demo\x07";
        // From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
        std::cout << "\n\n\n\x1b[2AHello,\x1b[1A\x1b[91mWorld!\x1b[2B\x1b[0m Здравствуй,\x1b[1A\x1b[92mмир!\x1b[0m\x1b[2B "; 
        std::cout << "Γεια σου \x1b[1A\x1b[94mκόσμε!\x1b[2B\x1b[0m\n";
        std::cout << "Type something: ";
    
        std::string input;
        std::getline(std::cin, input);
        std::cout << "You typed \x1b[45;1m" << input << "\x1b[0m.\n";
    
        return 0;
    }

Apart from the C++ header, a (GNU-flavoured) C header and Python module are also provided. 
The header files are MIT-licensed, so there's no licence worries when linking them to your project.

There are two ways to deploy it. One way:  

    E:\ANSIpipe> gcc launcher.c -o launcher.exe
    E:\ANSIpipe> g++ example.cpp -o example.exe
    E:\ANSIpipe> launcher example
    
    
The other way:  

    E:\ANSIpipe> gcc launcher.c -o example.com
    E:\ANSIpipe> g++ example.cpp -o example.exe
    E:\ANSIpipe> example

And this is what it looks like:

![Screenshot of ANSI|pipe in action](/../screenshots/screenshot.png?raw=true)


# How it does it

ANSI|pipe creates named pipes, one for each of `stdin`, `stdout` and `stderr`. 
It converts Windows keystrokes into a byte stream of UTF-8 and ANSI escape 
sequences, and sends this into the `stdin` pipe. At the same time, it takes the
input from the `stdout` and `stderr`, interprets and the ANSI sequences and
executes them on the Windows API, converts the text from UTF-8 to UTF-16 and 
prints it on the console.

As for the launcher, the Windows shell gives preference to `.COM` over `.EXE` 
when globbing an executable, all command-line access will naturally be routed 
through ANSI|pipe while any GUI access through the Start Menu or the File Manager 
can use the `.EXE` and avoid a console window. If ANSI|pipe sees its name ends 
in `.EXE`, it will take its first argument as the executable to run.   

Either way, any remaining command-line arguments are sent to your app's executable.  
For instance, a Python app that uses `import ansipipe` can be run with

    launcher python myapp.py


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
     
ANSI|pipe is free software, released under the GNU GPL 
(version [2](http://www.gnu.org/licenses/gpl-2.0.html) 
or [3](http://www.gnu.org/licenses/gpl-3.0.html)) and Perl's 
[Artistic License](http://dev.perl.org/licenses/artistic.html).
     
    
    


