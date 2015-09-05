ANSI|pipe code history
======================
https://github.com/robhagemans/ansipipe

ANSI|pipe is derived from the following sources:
* Win32::Console::ANSI Perl extension by J-L Morel
* dualsubsystem by gaber...@gmail.com, based on work by Richard Eperjesi
* further additions and modifications by Rob Hagemans

Win32::Console::ANSI
====================
http://www.bribes.org/perl/wANSIConsole.html

Modifications to Win32::Console::ANSI code
(RH 2015-01-27)
-   Remove DLL injection and API hooking code
-   Remove Win9x support code
-   Add #define macros to allow compilation under MinGW GCC
(RH 2015-01-28)
-   Change signature of ParseAndPrint to work with one hDev only
-   Remove codepage-conversion sequences ('\x1b(')
(RH 2015-01-30)
-   Add escape sequences for scrolling, terminal resizing, xterm bright colours
-   Add ANSI input routine (sends sequences for special keys)
-   Refactor code to allow multiple parsers (for cerr and cout)
In other words, I have extracted only the escape sequence parser from the
original source and then modified that.

dualsubsystem
=============
https://code.google.com/p/dualsubsystem/

Modifications to dualsysystem code
(RH 2015-01-28)
-   Change whitespace and naming conventions for readability
-   Replace command-line parsing code
-   Implement Unicode support, UTF8 conversions
-   Send stdout pipe output to ansi parser
