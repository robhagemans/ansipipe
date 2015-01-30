/*
ANSI|pipe SOURCE
================
ANSI|pipe is derived from the following sources:
* Win32::Console::ANSI Perl extension by J-L Morel
* dualsubsystem by gaber...@gmail.com, based on work by Richard Eperjesi
* further additions and modifications by Rob Hagemans

The original copyright and licence notices for the contributing sources
can be found below.

The modifications are copyright (c) 2015 Rob Hagemans.
The resulting file is released under the terms below.

LICENCE

This program is free software; you can redistribute it and/or modify
it under the terms of
1)  the GNU General Public License, version 2,
    as published by the Free Software Foundation; or, at your option,
2)  the GNU General Public License, version 3, 
    as published by the Free Software Foundation; or, at your option,
3)  the Artistic License as distributed with the Perl Kit.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. 
*/

/*
ORIGINAL SOURCE: Win32::Console::ANSI
=====================================
The following is a list of modifications of Win32::Console::ANSI
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

Below are the original copyright and licence notices of Win32::Console::ANSI.
Please note that the present modified file is released under slightly more 
restrictive terms, which are stated above. - RH

AUTHOR

J-L Morel <jl_morel@bribes.org>
Home page: http://www.bribes.org/perl/wANSIConsole.html

COPYRIGHT

Copyright (c) 2003-2014 J-L Morel. All rights reserved.

LICENCE

This distribution is free software; you can redistribute it and/or modify it
under the same terms as Perl itself, i.e. under the terms of either:

a) the GNU General Public License as published by the Free Software Foundation,
   either version 1, or (at your option) any later version; or

b) the Artistic License as distributed with the Perl Kit.

This distribution is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See either the GNU General Public License or the
Artistic License for more details.

You should have received a copy of the GNU General Public License with this
distribution in the file named "Copying".  If not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA or
visit their web page on the internet at http://www.gnu.org/licenses/gpl.html or
the Perl web page at http://dev.perl.org/licenses/gpl1.html.

You should also have received a copy of the Artistic License with this
distribution, in the file named "Artistic".  If not, visit the Perl web page on
the internet at http://dev.perl.org/licenses/artistic.html.
*/

/*
ORIGINAL SOURCE: dualsubsystem
==============================
The following is a list of modifications to dualsysystem
(RH 2015-01-28) 
-   Change whitespace and naming conventions for readability
-   Replace command-line parsing code
-   Implement Unicode support, UTF8 conversions
-   Send stdout pipe output to ansi parser

dualsubsystem can be found at https://code.google.com/p/dualsubsystem/.
I have not been able to find a copyright notice by dualsubsystem's author,
who is identified on Google Code as 
gaber...@gmail.com, https://code.google.com/u/112032256711997475328/. 
Below follow the licence and notices from this project that appear relevant.
Please note that the present modified file is released under more 
restrictive terms, which are stated above. - RH

SUMMARY

dualsubsystem is an update on a code written by Richard Eperjesi that provides 
the best of both worlds with the trick of using a same-named ".com" executable 
to handle the stdin/stdout/stderr redirection to the active console.

MIT LICENSE

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#define UNICODE
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <stdio.h>
#include <process.h> /*_beginthread, _endthread*/
#include <string.h>
#include <wctype.h>

// define bool, for C < C99
typedef enum { false, true } bool;

// not defined in MinGW
int wcscasecmp(wchar_t *a, wchar_t *b)
{
    while (*a && *b && towupper(*a++) == towupper(*b++));
    return (towupper(*a) != towupper(*b));
}


// handles to standard i/o streams
HANDLE handle_cout;
HANDLE handle_cin;
HANDLE handle_cerr;

// handles to named pipes
HANDLE cout_pipe;
HANDLE cin_pipe;
HANDLE cerr_pipe;


// ============================================================================
// Colour constants
// ============================================================================

#define FOREGROUND_BLACK 0
#define FOREGROUND_WHITE FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE

#define BACKGROUND_BLACK 0
#define BACKGROUND_WHITE BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE

int foregroundcolor[16] = {
      FOREGROUND_BLACK,                                       // black
      FOREGROUND_RED,                                         // red
      FOREGROUND_GREEN,                                       // green
      FOREGROUND_RED|FOREGROUND_GREEN,                        // yellow
      FOREGROUND_BLUE,                                        // blue
      FOREGROUND_BLUE|FOREGROUND_RED,                         // magenta
      FOREGROUND_BLUE|FOREGROUND_GREEN,                       // cyan
      FOREGROUND_WHITE,                                       // light grey
      FOREGROUND_BLACK|FOREGROUND_INTENSITY,                  // dark grey
      FOREGROUND_RED|FOREGROUND_INTENSITY,                    // bright red 
      FOREGROUND_GREEN|FOREGROUND_INTENSITY,                  // bright green
      FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,   // bright yellow
      FOREGROUND_BLUE|FOREGROUND_INTENSITY ,                  // bright blue
      FOREGROUND_BLUE|FOREGROUND_RED|FOREGROUND_INTENSITY,    // bright magenta
      FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_INTENSITY,  // bright cyan
      FOREGROUND_WHITE|FOREGROUND_INTENSITY                   // white
};

int backgroundcolor[16] = {
      BACKGROUND_BLACK,
      BACKGROUND_RED,
      BACKGROUND_GREEN,
      BACKGROUND_RED|BACKGROUND_GREEN,
      BACKGROUND_BLUE,
      BACKGROUND_BLUE|BACKGROUND_RED,
      BACKGROUND_BLUE|BACKGROUND_GREEN,
      BACKGROUND_WHITE,
      BACKGROUND_BLACK|BACKGROUND_INTENSITY,
      BACKGROUND_RED|BACKGROUND_INTENSITY,
      BACKGROUND_GREEN|BACKGROUND_INTENSITY,
      BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_INTENSITY,
      BACKGROUND_BLUE|BACKGROUND_INTENSITY,
      BACKGROUND_BLUE|BACKGROUND_RED|BACKGROUND_INTENSITY,
      BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_INTENSITY,
      BACKGROUND_WHITE|BACKGROUND_INTENSITY
};

// Table to convert the color order of the console in the ANSI order.
int conversion[16] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};

int foreground_default = FOREGROUND_WHITE;
int background_default = BACKGROUND_BLACK;

// ============================================================================
// Print Buffer
// ============================================================================

#define PBUF_SIZE 256

typedef struct {
    HANDLE handle;
    int nchar;
    wchar_t buf[PBUF_SIZE];
} PBUF;

//  write buffer to console
void pbuf_flush(PBUF *pbuf)
{
    if (pbuf->nchar <= 0) return;
    long written;
    WriteConsoleW(pbuf->handle, pbuf->buf, pbuf->nchar, &written, NULL);
    pbuf->nchar = 0;
}

//  add a character in the buffer and flush the buffer if it is full
void pbuf_push(PBUF *pbuf, wchar_t c)
{
    // skip NUL
    if (!c) return;
    pbuf->buf[pbuf->nchar++] = c;
    #ifdef ONLCR
    if (c == L'\r') pbuf[pbuf->nchar++] = L'\n';
    #endif
    // keep at least 2 wchars free so we can add CRLF at once if necessary
    if (pbuf->nchar >= PBUF_SIZE-1) pbuf_flush(pbuf);
}


// ============================================================================
// ANSI sequences
// ============================================================================

#define MAX_STRARG 1024         // max string arg length
#define MAX_ARG 16              // max number of args in an escape sequence

// current escape sequence state:
// for instance, with \e[33;45;1m we have
// prefix = '[',
// es.argc = 3, es.argv[0] = 33, es.argv[1] = 45, es.argv[2] = 1
// suffix = 'm'
typedef struct {
    char prefix;                    // escape sequence prefix ( '[' or '(' );
    char prefix2;                   // secondary prefix ( '?' );
    char suffix;                    // escape sequence suffix
    int argc;                    // escape sequence args count
    int argv[MAX_ARG];           // escape sequence args
    wchar_t args[MAX_STRARG];       // escape sequence string arg; length in argv[1]
} SEQUENCE;

// current attributes
typedef struct {
    int foreground;
    int background;
    bool concealed;
    bool bold;
    bool underline;
    bool rvideo;
    // scrolling
    SMALL_RECT scroll_region;
    // saved cursor position
    COORD save_pos;
} TERM;

// interpret the last escape sequence scanned by ansi_print()
void ansi_output(HANDLE handle, TERM *term, SEQUENCE es)
{
    int i;
    int attr;
    CONSOLE_SCREEN_BUFFER_INFO info;
    CONSOLE_CURSOR_INFO curs_info;
    long len, written;
    COORD pos;
    SMALL_RECT rect;
    CHAR_INFO char_info;
    
    if (es.prefix == '[') {
        if (es.prefix2 == '?' && (es.suffix == 'h' || es.suffix == 'l')) {
            if (es.argc == 1 && es.argv[0] == 25) {
                GetConsoleCursorInfo(handle, &curs_info);
                curs_info.bVisible = (es.suffix == 'h');
                SetConsoleCursorInfo(handle, &curs_info);
                return;
            }
        }
        // Ignore any other \e[? sequences.
        if (es.prefix2 != 0) return;

        GetConsoleScreenBufferInfo(handle, &info);
        switch (es.suffix) {
        case 'm':
            if (es.argc == 0) es.argv[es.argc++] = 0;
            for(i = 0; i < es.argc; i++) {
                switch (es.argv[i]) {
                case 0:
                    term->foreground = foreground_default;
                    term->background = background_default;
                    term->bold = false;
                    term->underline = false;
                    term->rvideo = false;
                    term->concealed = false;
                    break;
                case 1:
                    term->bold = true;
                    break;
                case 21:
                    term->bold = false;
                    break;
                case 4:
                    term->underline = true;
                    break;
                case 24:
                    term->underline = false;
                    break;
                case 7:
                    term->rvideo = true;
                    break;
                case 27:
                    term->rvideo = false;
                    break;
                case 8:
                    term->concealed = true;
                    break;
                case 28:
                    term->concealed = false;
                    break;
                }
                if ((100 <= es.argv[i]) && (es.argv[i] <= 107)) 
                    term->background = es.argv[i] - 100 + 8;
                else if ((90 <= es.argv[i]) && (es.argv[i] <= 97)) 
                    term->foreground = es.argv[i] - 90 + 8;
                else if ((40 <= es.argv[i]) && (es.argv[i] <= 47))
                    term->background = es.argv[i] - 40;
                else if ((30 <= es.argv[i]) && (es.argv[i] <= 37)) 
                    term->foreground = es.argv[i] - 30;
            }
            if (term->rvideo) 
                attr = foregroundcolor[term->background] | backgroundcolor[term->foreground];
            else 
                attr = foregroundcolor[term->foreground] | backgroundcolor[term->background];
            if (term->bold) 
                attr |= FOREGROUND_INTENSITY;
            if (term->underline) 
                attr |= BACKGROUND_INTENSITY;
            SetConsoleTextAttribute(handle, attr);
            return;
        case 'J':
            if (es.argc == 0) es.argv[es.argc++] = 0;   // ESC[J == ESC[0J
            if (es.argc != 1) return;
            switch (es.argv[0]) {
            case 0:              
                // ESC[0J erase from cursor to end of display
                len = (info.dwSize.Y-info.dwCursorPosition.Y-1)
                      *info.dwSize.X+info.dwSize.X-info.dwCursorPosition.X-1;
                FillConsoleOutputCharacter(handle, ' ', len,
                                    info.dwCursorPosition, &written);
                FillConsoleOutputAttribute(handle, info.wAttributes, len,
                                    info.dwCursorPosition, &written);
                return;
            case 1:              
                // ESC[1J erase from start to cursor.
                pos.X = 0;
                pos.Y = 0;
                len = info.dwCursorPosition.Y*info.dwSize.X+info.dwCursorPosition.X+1;
                FillConsoleOutputCharacter(handle, ' ', len, pos, 
                                                        &written);
                FillConsoleOutputAttribute(handle, info.wAttributes, len, pos, 
                                                        &written);
                return;
            case 2:              
                // ESC[2J Clear screen and home cursor
                pos.X = 0;
                pos.Y = 0;
                len = info.dwSize.X*info.dwSize.Y;
                FillConsoleOutputCharacter(handle, ' ', len, pos,
                                                        &written);
                FillConsoleOutputAttribute(handle, info.wAttributes, len, pos,
                                                        &written);
                SetConsoleCursorPosition(handle, pos);
                return;
            default :
                return;
            }
        case 'K':
            if (es.argc == 0) es.argv[es.argc++] = 0;   // ESC[K == ESC[0K
            if (es.argc != 1) return;
            switch (es.argv[0]) {
            case 0:              
                // ESC[0K Clear to end of line
                len = info.srWindow.Right-info.dwCursorPosition.X+1;
                FillConsoleOutputCharacter(handle, ' ', len, 
                                    info.dwCursorPosition, &written);
                FillConsoleOutputAttribute(handle, info.wAttributes, len,
                                    info.dwCursorPosition, &written);
                return;
            case 1:              
                // ESC[1K Clear from start of line to cursor
                pos.X = 0;
                pos.Y = info.dwCursorPosition.Y;
                FillConsoleOutputCharacter(handle, ' ', info.dwCursorPosition.X+1,
                                    pos, &written);
                FillConsoleOutputAttribute(handle, info.wAttributes, 
                                    info.dwCursorPosition.X+1, pos,
                                    &written);
                return;
            case 2:              
                // ESC[2K Clear whole line.
                pos.X = 0;
                pos.Y = info.dwCursorPosition.Y;
                FillConsoleOutputCharacter(handle, ' ', info.dwSize.X, 
                                                    pos, &written);
                FillConsoleOutputAttribute(handle, info.wAttributes, info.dwSize.X,
                                                    pos, &written);
                return;
            default :
                return;
            }
        case 'L':
            // ESC[#L Insert # blank lines.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[L == ESC[1L
            if (es.argc != 1) return;
            rect.Left   = 0;
            rect.Top    = info.dwCursorPosition.Y;
            rect.Right  = info.dwSize.X-1;
            rect.Bottom = info.dwSize.Y-1;
            pos.X = 0;
            pos.Y = info.dwCursorPosition.Y+es.argv[0];
            char_info.Char.AsciiChar = ' ';
            char_info.Attributes = info.wAttributes;
            ScrollConsoleScreenBuffer(handle, &rect, &term->scroll_region, pos, &char_info);
            pos.X = 0;
            pos.Y = info.dwCursorPosition.Y;
            FillConsoleOutputCharacter(handle, ' ', 
                            info.dwSize.X*es.argv[0], pos, &written);
            FillConsoleOutputAttribute(handle, info.wAttributes, 
                            info.dwSize.X*es.argv[0], pos, &written);
            return;
        case 'M':
            // ESC[#M Delete # line.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[M == ESC[1M
            if (es.argc != 1) return;
            if (es.argv[0] > info.dwSize.Y - info.dwCursorPosition.Y)
                es.argv[0] = info.dwSize.Y - info.dwCursorPosition.Y;
            rect.Left   = 0;
            rect.Top    = info.dwCursorPosition.Y+es.argv[0];
            rect.Right  = info.dwSize.X-1;
            rect.Bottom = info.dwSize.Y-1;
            pos.X = 0;
            pos.Y = info.dwCursorPosition.Y;
            char_info.Char.AsciiChar = ' ';
            char_info.Attributes = info.wAttributes;
            ScrollConsoleScreenBuffer(handle, &rect, &term->scroll_region, pos, &char_info);
            pos.Y = info.dwSize.Y - es.argv[0];
            FillConsoleOutputCharacter(handle, ' ', 
                            info.dwSize.X * es.argv[0], pos, &written);
            FillConsoleOutputAttribute(handle, info.wAttributes,
                            info.dwSize.X * es.argv[0], pos, &written);
            return;
        case 'P':
            // ESC[#P Delete # characters.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[P == ESC[1P
            if (es.argc != 1) return;
            if (info.dwCursorPosition.X + es.argv[0] > info.dwSize.X - 1)
                es.argv[0] = info.dwSize.X - info.dwCursorPosition.X;
            rect.Left   = info.dwCursorPosition.X + es.argv[0];
            rect.Top    = info.dwCursorPosition.Y;
            rect.Right  = info.dwSize.X-1;
            rect.Bottom = info.dwCursorPosition.Y;
            char_info.Char.AsciiChar = ' ';
            char_info.Attributes = info.wAttributes;
            ScrollConsoleScreenBuffer(handle, &rect, &term->scroll_region, 
                                                info.dwCursorPosition, &char_info);
            pos.X = info.dwSize.X - es.argv[0];
            pos.Y = info.dwCursorPosition.Y;
            FillConsoleOutputCharacter(handle, ' ', es.argv[0], pos,
                                                            &written);
            return;
        case '@':
            // ESC[#@ Insert # blank characters.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[@ == ESC[1@
            if (es.argc != 1) return;
            if (info.dwCursorPosition.X + es.argv[0] > info.dwSize.X - 1)
                es.argv[0] = info.dwSize.X - info.dwCursorPosition.X;
            rect.Left   = info.dwCursorPosition.X;
            rect.Top    = info.dwCursorPosition.Y;
            rect.Right  = info.dwSize.X-1-es.argv[0];
            rect.Bottom = info.dwCursorPosition.Y;
            pos.X = info.dwCursorPosition.X+es.argv[0];
            pos.Y = info.dwCursorPosition.Y;
            char_info.Char.AsciiChar = ' ';
            char_info.Attributes = info.wAttributes;
            ScrollConsoleScreenBuffer(handle, &rect, &term->scroll_region, 
                                                               pos, &char_info);
            FillConsoleOutputCharacter(handle, ' ', es.argv[0],
                                    info.dwCursorPosition, &written);
            FillConsoleOutputAttribute(handle, info.wAttributes, es.argv[0],
                                    info.dwCursorPosition, &written);
            return;
        case 'A':
            // ESC[#A Moves cursor up # lines
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[A == ESC[1A
            if (es.argc != 1) return;
            pos.X = info.dwCursorPosition.X;
            pos.Y = info.dwCursorPosition.Y-es.argv[0];
            if (pos.Y < 0) pos.Y = 0;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'B':
            // ESC[#B Moves cursor down # lines
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[B == ESC[1B
            if (es.argc != 1) return;
            pos.X = info.dwCursorPosition.X;
            pos.Y = info.dwCursorPosition.Y+es.argv[0];
            if (pos.Y >= info.dwSize.Y) pos.Y = info.dwSize.Y-1;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'C':
            // ESC[#C Moves cursor forward # spaces
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[C == ESC[1C
            if (es.argc != 1) return;
            pos.X = info.dwCursorPosition.X+es.argv[0];
            if (pos.X >= info.dwSize.X) pos.X = info.dwSize.X-1;
            pos.Y = info.dwCursorPosition.Y;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'D':
            // ESC[#D Moves cursor back # spaces
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[D == ESC[1D
            if (es.argc != 1) return;
            pos.X = info.dwCursorPosition.X-es.argv[0];
            if (pos.X < 0) pos.X = 0;
            pos.Y = info.dwCursorPosition.Y;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'E':
            // ESC[#E Moves cursor down # lines, column 1.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[E == ESC[1E
            if (es.argc != 1) return;
            pos.X = 0;
            pos.Y = info.dwCursorPosition.Y+es.argv[0];
            if (pos.Y >= info.dwSize.Y) pos.Y = info.dwSize.Y-1;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'F':
            // ESC[#F Moves cursor up # lines, column 1.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[F == ESC[1F
            if (es.argc != 1) return;
            pos.X = 0;
            pos.Y = info.dwCursorPosition.Y-es.argv[0];
            if (pos.Y < 0) pos.Y = 0;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'G':
            // ESC[#G Moves cursor column # in current row.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[G == ESC[1G
            if (es.argc != 1) return;
            pos.X = es.argv[0] - 1;
            if (pos.X >= info.dwSize.X) pos.X = info.dwSize.X-1;
            if (pos.X < 0) pos.X = 0;
            pos.Y = info.dwCursorPosition.Y;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 'f':
        case 'H':
            // ESC[#;#H or ESC[#;#f Moves cursor to line #, column #
            if (es.argc == 0) {
                es.argv[es.argc++] = 1;   // ESC[G == ESC[1;1G
                es.argv[es.argc++] = 1;
            }
            if (es.argc == 1) {
                es.argv[es.argc++] = 1;   // ESC[nG == ESC[n;1G
            }
            if (es.argc > 2) return;
            pos.X = es.argv[1] - 1;
            if (pos.X < 0) pos.X = 0;
            if (pos.X >= info.dwSize.X) pos.X = info.dwSize.X-1;
            pos.Y = es.argv[0] - 1;
            if (pos.Y < 0) pos.Y = 0;
            if (pos.Y >= info.dwSize.Y) pos.Y = info.dwSize.Y-1;
            SetConsoleCursorPosition(handle, pos);
            return;
        case 's':
            // ESC[s Saves cursor position for recall later
            if (es.argc != 0) return;
            term->save_pos.X = info.dwCursorPosition.X;
            term->save_pos.Y = info.dwCursorPosition.Y;
            return;
        case 'u':
            // ESC[u Return to saved cursor position
            if (es.argc != 0) return;
            SetConsoleCursorPosition(handle, term->save_pos);
            return;
        case 'r':
            // ESC[r set scroll region
            if (es.argc == 0) {
                // ESC[r == ESC[top;botr
                term->scroll_region.Top    = 0;
                term->scroll_region.Bottom = info.dwSize.Y - 1;
            }
            else if (es.argc == 2) {
                term->scroll_region.Top    = es.argv[0] - 1;
                term->scroll_region.Bottom = es.argv[1] - 1;
            }
            return;
        case 'S':
            // ESC[#S scroll up # lines
            if (es.argc != 1) return;
            rect = term->scroll_region;
            rect.Top = es.argv[0];
            rect.Bottom = info.dwSize.Y - 1;
            pos.X = 0;
            pos.Y = 0;
            char_info.Char.AsciiChar = ' ';
            char_info.Attributes = info.wAttributes;
            ScrollConsoleScreenBuffer(handle, &rect, &term->scroll_region, 
                                                                pos, &char_info);
                    pos.X = 0;
            pos.X = 0;
            pos.Y = term->scroll_region.Bottom;
            FillConsoleOutputCharacter(handle, ' ', 
                            info.dwSize.X*es.argv[0], pos, &written);
            FillConsoleOutputAttribute(handle, info.wAttributes, 
                            info.dwSize.X*es.argv[0], pos, &written);
            return;
        case 'T':
            // ESC[#T scroll down # lines
            if (es.argc != 1) return;
            rect = term->scroll_region;
            rect.Top = 0;
            rect.Bottom = info.dwSize.Y - es.argv[0];
            pos.X = 0;
            pos.Y = es.argv[0];
            char_info.Char.AsciiChar = ' ';
            char_info.Attributes = info.wAttributes;
            ScrollConsoleScreenBuffer(handle, &rect, &term->scroll_region, 
                                                                pos, &char_info);
            pos.X = 0;
            pos.Y = term->scroll_region.Top;
            FillConsoleOutputCharacter(handle, ' ', 
                            info.dwSize.X*es.argv[0], pos, &written);
            FillConsoleOutputAttribute(handle, info.wAttributes, 
                            info.dwSize.X*es.argv[0], pos, &written);
            return;
        case 't':
            //ESC[8;#;#;t resize terminal to # rows, # cols
            if (es.argc < 3) return;
            if (es.argv[0] != 8) return;
            pos.X = es.argv[2];
            pos.Y = es.argv[1];
            rect.Top = 0;
            rect.Left = 0;
            rect.Bottom = es.argv[1] - 1;
            rect.Right = es.argv[2] - 1;
            SetConsoleScreenBufferSize(handle, pos);
            SetConsoleWindowInfo(handle, true, &rect);
            // do it twice, because one call can only make the console bigger 
            // and the other call can only make it smaller. plus I am lazy.
            SetConsoleScreenBufferSize(handle, pos);
            SetConsoleWindowInfo(handle, true, &rect);
            return;
        }
    }
    else if (es.prefix == ']' && es.suffix == '\x07') {
        if (es.argc != 2) return;
        switch (es.argv[0]) {
            case 2:
                // ESC]2;%sBEL: set title
                SetConsoleTitle(es.args);
        }            
    }
}

// retrieve utf-8 and ansi sequences from standard input
bool ansi_input(char *buffer, long buflen, long *count)
{
    // event buffer size: 
    // -  for utf8, buflen/4 is OK as one wchar is at most 4 chars of utf8
    // -  escape codes are at most 5 ascii-128 wchars; translate into 5 chars
    // so buflen/5 events should fit in buflen wchars and buflen utf8 chars.
    wchar_t wide_buffer[buflen];
    INPUT_RECORD events[buflen / 5];
    long ecount;
    if (!ReadConsoleInput(handle_cin, events, buflen / 5, &ecount))
        return false;
    int i;
    int wcount = 0;
    for (i = 0; i < ecount; ++i) {
        if (events[i].EventType == KEY_EVENT) {
            if (events[i].Event.KeyEvent.bKeyDown) {
                // insert ansi escape codes for arrow keys etc.
                switch (events[i].Event.KeyEvent.wVirtualKeyCode) {
                case VK_PRIOR:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x35';
                    wide_buffer[wcount++] = L'\x7e';
                    break;

                case VK_NEXT:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x36';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_END:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x4f';
                    wide_buffer[wcount++] = L'\x46';
                    break;
                case VK_HOME:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x4f';
                    wide_buffer[wcount++] = L'\x48';
                    break;
                case VK_LEFT:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x44';
                    break;
                case VK_UP:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x41';
                    break;
                case VK_RIGHT:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x43';
                    break;
                case VK_DOWN:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x42';
                    break;
                case VK_INSERT:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x32';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_DELETE:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x33';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F1:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x4f';
                    wide_buffer[wcount++] = L'\x50';
                    break;
                case VK_F2:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x4f';
                    wide_buffer[wcount++] = L'\x51';
                    break;
                case VK_F3:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x4f';
                    wide_buffer[wcount++] = L'\x52';
                    break;
                case VK_F4:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x4f';
                    wide_buffer[wcount++] = L'\x53';
                    break;
                case VK_F5:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x31';
                    wide_buffer[wcount++] = L'\x35';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F6:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x31';
                    wide_buffer[wcount++] = L'\x37';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F7:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x31';
                    wide_buffer[wcount++] = L'\x38';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F8:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x31';
                    wide_buffer[wcount++] = L'\x39';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F9:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x32';
                    wide_buffer[wcount++] = L'\x30';
                    wide_buffer[wcount++] = L'\x7e';
                    break;

                case VK_F10:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x32';
                    wide_buffer[wcount++] = L'\x31';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F11:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x32';
                    wide_buffer[wcount++] = L'\x33';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                case VK_F12:
                    wide_buffer[wcount++] = L'\x1b';
                    wide_buffer[wcount++] = L'\x5b';
                    wide_buffer[wcount++] = L'\x31';
                    wide_buffer[wcount++] = L'\x34';
                    wide_buffer[wcount++] = L'\x7e';
                    break;
                default:
                    wide_buffer[wcount++] = events[i].Event.KeyEvent.uChar.UnicodeChar;
                    #ifdef ECHO
                    if (wide_buffer[wcount-1] == L'\r')
                        printf("\n");
                    else
                        printf("%lc", wide_buffer[wcount-1]);
                    #endif
                }
                // safety check
                if (wcount >= buflen / 4) {
                    fprintf(stderr, "ERROR: Input buffer overflow.\n");
                    return false;
                }
                #ifdef ICRNL
                // CR -> LF
                if (wide_buffer[wcount-1] == L'\r') 
                    wide_buffer[wcount-1] = L'\n';
                #endif
            }
        }
    }
    wide_buffer[wcount] = 0;
    // find UTF8 string length    
    int length = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1, NULL, 0, NULL, NULL);
    // safety check
    if (length >= buflen) {
        fprintf(stderr, "ERROR: UTF-8 buffer overflow.\n");
        return false;
    }
    // convert to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1, buffer, length, NULL, NULL);
    // exclude NUL terminator in reported num chars
    *count = length-1;
    return true;
}


// ============================================================================
// Parser
// ============================================================================

typedef struct {
    HANDLE handle;
    SEQUENCE es;
    TERM term;
    PBUF pbuf;
    int state;
} PARSER;

// initialise a new ansi sequence parser
void parser_init(PARSER *p, HANDLE handle)
{
    p->handle = handle;
    p->pbuf.handle = handle;
    p->state = 1;
    p->term.foreground = foreground_default;
    p->term.background = background_default;
    // initialise scroll region to full screen
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(handle, &info);
    p->term.scroll_region.Left   = 0;
    p->term.scroll_region.Right  = info.dwSize.X - 1;
    p->term.scroll_region.Top    = 0;
    p->term.scroll_region.Bottom = info.dwSize.Y - 1;
    // initialise escape sequence
    p->es.prefix = 0;
    p->es.prefix2 = 0;
    p->es.suffix = 0;
    p->es.argc = 0;
    p->es.args[0] = 0;
}

// Parse the string buffer, interpret the escape sequences and print the
// characters on the console.
// If the number of arguments es.argc > MAX_ARG, only the MAX_ARG-1 firsts and
// the last arguments are processed (no es.argv[] overflow).
void parser_print(PARSER *p, char *buffer)
{
    // get required buffer length
    int length = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
    wchar_t wide_buffer[length];
    // convert UTF-8 -> UTF-16
    // utf8 sequences could be clipped if buffer is full, doesn't seem to happen
    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wide_buffer, length);
    // start parsing
    int i;
    wchar_t *s;
    for (i = length, s = wide_buffer; i > 0 && *s; i--, s++) {
        switch (p->state) {
        case 1:
            if (*s == L'\x1b') p->state = 2;
            else pbuf_push(&(p->pbuf), p->term.concealed ? ' ' : *s);
            break;
        case 2:
            if (*s == L'\x1b');       // \e\e...\e == \e
            else if (*s == '[') {
                pbuf_flush(&(p->pbuf));
                p->es.prefix = *s;
                p->es.prefix2 = 0;
                p->state = 3;
            }
            else if (*s == ']') {
                pbuf_flush(&(p->pbuf));
                p->es.prefix = *s;
                p->es.prefix2 = 0;
                p->es.argc = 0;
                p->es.argv[0] = 0;
                p->state = 5;
            }
            else p->state = 1;
            break;
        case 3:
            if (isdigit(*s)) {
                p->es.argc = 0;
                p->es.argv[0] = *s-'0';
                p->state = 4;
            }
            else if (*s == ';') {
                p->es.argc = 1;
                p->es.argv[0] = 0;
                p->es.argv[p->es.argc] = 0;
                p->state = 4;
            }
            else if (*s == '?') {
                p->es.prefix2 = *s;
            }
            else {
                p->es.argc = 0;
                p->es.suffix = *s;
                ansi_output(p->handle, &(p->term), p->es);
                p->state = 1;
            }
            break;
        case 4:
            if (isdigit(*s)) {
                p->es.argv[p->es.argc] = 10*p->es.argv[p->es.argc]+(*s-'0');
            }
            else if (*s == ';') {
                if (p->es.argc < MAX_ARG-1) p->es.argc++;
                    p->es.argv[p->es.argc] = 0;
            }
            else {
                if (p->es.argc < MAX_ARG-1) p->es.argc++;
                p->es.suffix = *s;
                ansi_output(p->handle, &(p->term), p->es);
                p->state = 1;
            }
            break;
        case 5:
            // ESC]%d;%sBEL
            if (isdigit(*s)) {
                p->es.argc = 1;
                p->es.argv[0] = 10*p->es.argv[0]+(*s-'0');
            }
            else if (*s == ';') {
                p->es.argc = 2;
                p->es.argv[1] = 0;
                p->state = 6;    
            }
            else {
                p->es.suffix = *s;
                ansi_output(p->handle, &(p->term), p->es);
                p->state = 1;
            }
            break;
        case 6:
            // read string argument
            if (*s != '\x07' && p->es.argv[1] < MAX_STRARG-1) {
                p->es.args[p->es.argv[1]++] = *s;
            }
            else {
                p->es.args[p->es.argv[1]] = 0;
                p->es.suffix = *s;
                ansi_output(p->handle, &(p->term), p->es);
                p->state = 1;
            }
        }
    }
    pbuf_flush(&(p->pbuf));
}



// ============================================================================
// named pipes 
// ============================================================================

// pipe globals
#define PIPES_TIMEOUT 1000
#define PIPES_BUFLEN 1024


// Create named pipes for stdin, stdout and stderr
// Parameter: process id
bool pipes_create(long pid) 
{
    wchar_t name[256];
    swprintf(name, L"\\\\.\\pipe\\%dcout", pid);
    if (INVALID_HANDLE_VALUE == (cout_pipe = CreateNamedPipe(
                name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                1, PIPES_BUFLEN, PIPES_BUFLEN, PIPES_TIMEOUT, NULL)))
        return 0;
    swprintf(name, L"\\\\.\\pipe\\%dcin", pid);
    if (INVALID_HANDLE_VALUE == (cin_pipe = CreateNamedPipe(
                name, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                1, PIPES_BUFLEN, PIPES_BUFLEN, PIPES_TIMEOUT, NULL)))
        return 0;
    swprintf(name, L"\\\\.\\pipe\\%dcerr", pid);
    if (INVALID_HANDLE_VALUE == (cerr_pipe = CreateNamedPipe(
                name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                1, PIPES_BUFLEN, PIPES_BUFLEN, PIPES_TIMEOUT, NULL)))
        return 0;
    return 1;
}

// Close all named pipes
void pipes_close() 
{
    // Give pipes a chance to flush
    Sleep(100);
    CloseHandle(cout_pipe);
    CloseHandle(cerr_pipe);
    CloseHandle(cin_pipe);
}

// Thread function that handles incoming bytestreams to be output on stdout
void pipes_cout_thread(void *dummy) 
{
    // we're sending UTF-8 through these pipes
    char buffer[PIPES_BUFLEN];
    long count = 0;
    PARSER p = {};
    parser_init(&p, handle_cout);
    ConnectNamedPipe(cout_pipe, NULL);
    while (ReadFile(cout_pipe, buffer, PIPES_BUFLEN-1, &count, NULL)) {
        buffer[count] = 0;
        parser_print(&p, buffer);
    }
}

// Thread function that handles incoming bytestreams to be outputed on stderr
void pipes_cerr_thread(void *dummy) 
{
    char buffer[PIPES_BUFLEN];
    long count = 0;
    PARSER p = {};
    parser_init(&p, handle_cerr);
    ConnectNamedPipe(cerr_pipe, NULL);
    while (ReadFile(cerr_pipe, buffer, PIPES_BUFLEN-1, &count, NULL)) {
        buffer[count] = 0;
        parser_print(&p, buffer);
//        // no ansi escape sequences for stderr
//        utf8_fprint(stderr, buffer);
    }
}

// Thread function that handles incoming bytestreams from stdin
void pipes_cin_thread(void *dummy)
{
    char buffer[PIPES_BUFLEN];
    long countr = 0;
    long countw = 0;
    ConnectNamedPipe(cin_pipe, NULL);
    for(;;) {
        if (!ansi_input(buffer, PIPES_BUFLEN-1, &countr))
            break;
        if (!WriteFile(cin_pipe, buffer, countr, &countw, NULL))
            break;
    }
}

// Start handler pipe handler threads
void pipes_start_threads()
{
    _beginthread(pipes_cin_thread, 0, NULL);
    _beginthread(pipes_cout_thread, 0, NULL);
    _beginthread(pipes_cerr_thread, 0, NULL);
}



// ============================================================================
// main function
// ============================================================================

// buffer for command-line arguments to child process
#define ARG_BUFLEN 2048

// create command line for child process
void build_command_line(int argc, char *argv[], wchar_t *buffer, long buflen)
{
    // get full program name, exclude extension
    wchar_t module_name[MAX_PATH+1];
    GetModuleFileName(0, module_name, MAX_PATH);
    module_name[MAX_PATH] = 0;
    int module_len = wcslen(module_name);
    // start with empty string
    buffer[0] = L'\0';
    int count = 0;
    // only call X.EXE if we're named X.COM
    // if we're an EXE the first argument is the child executable, so skip this
    if (module_len > 4 && wcscasecmp(module_name + module_len - 4, L".exe")) {
        module_name[module_len - 4] = 0;
        count += module_len + 1;
        if (count > buflen) {
            fprintf(stderr, "ERROR: Application name too long.\n");
            return;
        }
        wcscat(buffer, module_name);
        wcscat(buffer, L".exe ");
    }
    // write all arguments to child command line
    int i = 0;
    for (i = 1; i < argc; ++i) {
        int length = MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, NULL, 0);
        wchar_t wide_buffer[length];
        // convert UTF-8 -> UTF-16
        MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, wide_buffer, length);
        count += length + 1;
        if (count >= buflen) break;
        wcscat(buffer, wide_buffer);
        wcscat(buffer, L" ");
    }
}

int main(int argc, char *argv[])
{
    handle_cout = GetStdHandle(STD_OUTPUT_HANDLE);
    handle_cin = GetStdHandle(STD_INPUT_HANDLE);
    handle_cerr = GetStdHandle(STD_ERROR_HANDLE);


    /* save initial console attributes */

    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(handle_cout, &info);

    /* build command line */

    wchar_t cmd_line[ARG_BUFLEN];
    build_command_line(argc, argv, cmd_line, ARG_BUFLEN);

    /* start pipes */
    
    // spawn child process in suspended mode and create pipes
    PROCESS_INFORMATION pinfo;
    STARTUPINFO sinfo;
    memset(&sinfo, 0, sizeof(STARTUPINFO));
    sinfo.cb = sizeof(STARTUPINFO);
    if (!CreateProcess(NULL, cmd_line, NULL, NULL, FALSE, 
                       CREATE_SUSPENDED, NULL, NULL, &sinfo, &pinfo)) {
        fprintf(stderr, "ERROR: Could not create process.\n");
        return 1;
    }
    if (!pipes_create(pinfo.dwProcessId)) {
        fprintf(stderr, "ERROR: Could not create named pipes.\n");
        return 1;
    }

    // save console input mode to restore at exit
    long save_mode;
    GetConsoleMode(handle_cin, &save_mode);

    // set mode to accept keyboard event only
    long mode = 0; //ENABLE_WINDOW_INPUT
    SetConsoleMode(handle_cin, mode);
    
    // start the pipe threads and resume child process
    pipes_start_threads();
    ResumeThread(pinfo.hThread);

    /* wait for process */

    // wait for child process to exit and close pipes
    WaitForSingleObject(pinfo.hProcess, INFINITE);
    long exit_code;
    GetExitCodeProcess(pinfo.hProcess, &exit_code);

    /* close pipes */
    
    pipes_close();
    // restore console mode
    SetConsoleMode(handle_cin, save_mode);

    /* restore console */
    
    // restore console attributes
    SetConsoleTextAttribute(handle_cout, info.wAttributes);
    // restore screen buffer size
    SetConsoleScreenBufferSize(handle_cout, info.dwSize);
    
    return exit_code;
}
