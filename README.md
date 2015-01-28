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

## What it does

ANSI|pipe consists of a small binary that you can distribute with your compiled
code. It intercepts the standard I/O streams, converting between UTF-8 on the
app's side and UTF-16 on Windows' side and translating any ANSI sequences 
the apps write to its standard output into Windows console API calls. To port
to Windows, application code only needs to include one tiny header file and 
call a single initialising function, which re-routes standard I/O to a named
pipe.  

## How to use it

Give the ANSI|pipe binary the same name as your application's main .exe 
binary, but with a .COM extension. As the Windows shell gives preference to .COM 
over .EXE when globbing an executable, all command-line access will naturally
be routed through ANSI|pipe while any GUI access through the Start Menu or 
the File Manager can use the .EXE and avoid a console window.  

## Acknowledgements

ANSI|pipe merges part of two existing projects:
-   [**Win32::Console::ANSI**](http://search.cpan.org/~jlmorel/Win32-Console-ANSI-1.08/lib/Win32/Console/ANSI.pm)
    by **J-L Morel** that implements ANSI escape 
    sequences on the Windows console for Perl.  
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
     
    
    


