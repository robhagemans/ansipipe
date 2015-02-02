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

#ifndef _WIN32

#ifndef ANSIPIPE_SINGLE
int main() {}
#endif

#else

#define UNICODE
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <wctype.h>
#include "ansipipe.h"

// define bool, for C < C99
typedef enum { false, true } bool;

// not defined in MinGW
int wcscasecmp(wchar_t *a, wchar_t *b)
{
    while (*a && *b && towupper(*a++) == towupper(*b++));
    return (*a || *b);
}



// ============================================================================
// console globals
// ============================================================================

// handles to standard i/o streams
HANDLE handle_cout;
HANDLE handle_cin;
HANDLE handle_cerr;

// termios-style flags
typedef struct {
    int echo;
    int icrnl;
    int onlcr;
} FLAGS;

FLAGS flags = { true, true, false };



// ============================================================================
// colour constants
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
// string cat
// ============================================================================

typedef struct {
    wchar_t *buffer;
    long size;
    long count;
} WSTR;

WSTR wstr_create_empty(wchar_t *buffer, long size) {
    WSTR newstr;
    if (size > 0)
        buffer[0] = 0;
    newstr.buffer = buffer;
    newstr.size = size;
    newstr.count = 0;
    return newstr;
}

// instr is a null-terminated string
wchar_t* wstr_write(WSTR *wstr, wchar_t *instr, long length)
{
    // length does not include the null terminator; size does
    if (wstr->count + length + 1 > wstr->size) {
        wcsncpy(wstr->buffer, instr, wstr->size - wstr->count - 1);
        wstr->buffer[wstr->count-1] = 0;
        wstr->count = wstr->size;
        return NULL;
    }
    wcscpy(wstr->buffer + wstr->count, instr);
    wstr->count += length;
    return wstr->buffer + wstr->count;
}

wchar_t* wstr_write_char(WSTR *wstr, wchar_t c)
{
    if (wstr->count + 2 > wstr->size) {
        wstr->count = wstr->size;
        return NULL;
    }
    wstr->buffer[wstr->count++] = c;
    wstr->buffer[wstr->count] = 0;
    return wstr->buffer + wstr->count;
}

// ============================================================================
// print buffer
// ============================================================================

// max length of UTF-16 buffer
#define PBUF_SIZE 256
// max length of utf-8 sequence is 4 bytes
#define PBUF_PREBUF_SIZE 8

typedef struct {
    HANDLE handle;
    int count;
    wchar_t buf[PBUF_SIZE];
    char prebuf[PBUF_PREBUF_SIZE];
    int precount;
    int presize;
} PBUF;


//  write buffer to console
void pbuf_flush(PBUF *pbuf)
{
    if (pbuf->count <= 0) return;
    long written;
//    WriteConsoleW(pbuf->handle, pbuf->buf, pbuf->count, &written, NULL);
    pbuf->count = 0;
}


COORD onebyone = { 1, 1 };
COORD origin = { 0, 0 };

wchar_t hold = 0;

void console_put_char(HANDLE handle, wchar_t *s)
{
    // TODO: we need to get hold of a TERM pointer, so all of this is already there
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(handle, &info);
    // do not advance cursor if we're on the last position of the screen buffer, to avoid unwanted scrolling.
    if (!hold & info.dwCursorPosition.Y == info.dwSize.Y-1 && info.dwCursorPosition.X == info.dwSize.X-1) {
        SMALL_RECT dest = { info.dwCursorPosition.X, info.dwCursorPosition.Y, info.dwCursorPosition.X, info.dwCursorPosition.Y };
        CHAR_INFO ch;
        ch.Char.UnicodeChar = s[0];
        ch.Attributes = info.wAttributes;
        WriteConsoleOutput(handle, &ch, onebyone, origin, &dest);
        hold = s[0];
    }
    else {
        long written;
        if (hold) {
            // write the held character in the normal way, so cursor advances etc.
            WriteConsole(handle, &hold, 1, &written, NULL);
            hold = 0;
        }
        WriteConsole(handle, s, 1, &written, NULL);
    }    
}

// convert prebuffer UTF-8 sequence into one wide char for buffer
void pbuf_preflush(PBUF *pbuf)
{
    wchar_t wide_buffer[2] = L"";
    // utf8 sequences could be clipped if buffer is full, doesn't seem to happen
    MultiByteToWideChar(CP_UTF8, 0, pbuf->prebuf, pbuf->precount, wide_buffer, 1);
    wide_buffer[1] = 0;
    pbuf->buf[pbuf->count++] = wide_buffer[0];
    pbuf->precount = 0;
    if (flags.onlcr && wide_buffer[0] == L'\r') 
        pbuf->buf[pbuf->count++] = L'\n';
    console_put_char(pbuf->handle, wide_buffer);
}

//  add a character in the buffer and flush the buffer if it is full
void pbuf_push(PBUF *pbuf, unsigned char c)
{
    // skip NUL
    if (!c) return;
    pbuf->prebuf[pbuf->precount++] = c;
    if (1 == pbuf->precount) {
        // first byte determines sequence length
        if (c >= 0xf0) pbuf->presize = 4;
        else if (c >= 0xe0) pbuf->presize = 3;
        else if (c >= 0xc0) pbuf->presize = 2;
        else pbuf->presize = 1;
    }
    if (pbuf->precount >= pbuf->presize) pbuf_preflush(pbuf);
    // keep at least 2 wchars free so we can add CRLF at once if necessary
    if (pbuf->count >= PBUF_SIZE-1) pbuf_flush(pbuf);
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
    // terminal attributes
    int col;
    int row;
    int width;
    int height;
    int attr;
    HANDLE handle;
} TERM;


void console_fill(TERM *term, int x, int y, int len)
{
    long written;
    COORD pos;
    pos.X = x;
    pos.Y = y;
    FillConsoleOutputCharacter(term->handle, ' ', len, pos, &written);
    FillConsoleOutputAttribute(term->handle, term->attr, len, pos, &written);
}

void console_scroll(TERM *term, int left, int top, int right, int bot, int x, int y)
{
    SMALL_RECT rect;
    rect.Left = left;
    rect.Top = top;
    rect.Right = right;
    rect.Bottom = bot;
    CHAR_INFO char_info;
    char_info.Char.AsciiChar = ' ';
    char_info.Attributes = term->attr;
    COORD pos;
    pos.X = x;
    pos.Y = y;
    if (term->scroll_region.Bottom == term->height-2 && bot >= term->height-2 && y < top) {
        // workaround: in this particular case, Windows doesn't seem to respect the clip area.
        // first scroll everything up
        SMALL_RECT temp_scr = term->scroll_region;
        temp_scr.Bottom = term->height-1;
        rect.Bottom = term->height-1;
        ScrollConsoleScreenBuffer(term->handle, &rect, &temp_scr, pos, &char_info);
        // and then scroll the bottom back down
        pos.Y = term->height-1;
        rect.Top = term->height-1 - (top-y);
        rect.Bottom = rect.Top;
        ScrollConsoleScreenBuffer(term->handle, &rect, &temp_scr, pos, &char_info);
    }
    else
        ScrollConsoleScreenBuffer(term->handle, &rect, &(term->scroll_region), pos, &char_info);
}

void console_set_pos(TERM *term, int x, int y)
{
    if (y < 0) y = 0;
    else if (y >= term->height) y = term->height - 1;
    if (x < 0) x = 0;
    else if (x >= term->width) x = term->width - 1;
    COORD pos;
    pos.X = x;
    pos.Y = y;
    SetConsoleCursorPosition(term->handle, pos);
}



// interpret the last escape sequence scanned by ansi_print()
void ansi_output(TERM *term, SEQUENCE es)
{
    if (es.prefix == '[') {
        if (es.prefix2 == '?' && (es.suffix == 'h' || es.suffix == 'l')) {
            if (es.argc == 1 && es.argv[0] == 25) {
                CONSOLE_CURSOR_INFO curs_info;
                GetConsoleCursorInfo(term->handle, &curs_info);
                curs_info.bVisible = (es.suffix == 'h');
                SetConsoleCursorInfo(term->handle, &curs_info);
                return;
            }
        }
        // Ignore any other \e[? sequences.
        if (es.prefix2 != 0) return;
        // retrieve current positions and sizes
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(term->handle, &info);
        term->col = info.dwCursorPosition.X;
        term->row = info.dwCursorPosition.Y;
        term->width = info.dwSize.X;
        term->height = info.dwSize.Y;
        term->attr = info.wAttributes;
        
        switch (es.suffix) {
        case 'm':
            if (es.argc == 0) es.argv[es.argc++] = 0;
            int i;
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
            int new_attr;
            if (term->rvideo) 
                new_attr = foregroundcolor[term->background] | backgroundcolor[term->foreground];
            else 
                new_attr = foregroundcolor[term->foreground] | backgroundcolor[term->background];
            if (term->bold) 
                new_attr |= FOREGROUND_INTENSITY;
            if (term->underline) 
                new_attr |= BACKGROUND_INTENSITY;
            SetConsoleTextAttribute(term->handle, new_attr);
            return;
        case 'J':
            if (es.argc == 0) es.argv[es.argc++] = 0;   // ESC[J == ESC[0J
            if (es.argc != 1) return;
            switch (es.argv[0]) {
            case 0:              
                // ESC[0J erase from cursor to end of display
                console_fill(term, term->col, term->row, 
                        (term->height-term->row-1)*term->width + term->width-term->col-1);
                return;
            case 1:              
                // ESC[1J erase from start to cursor.
                console_fill(term, 0, 0, term->row*term->width + term->col + 1);
                return;
            case 2:              
                // ESC[2J Clear screen and home cursor
                console_fill(term, 0, 0, term->width * term->height);
                console_set_pos(term, 0, 0);
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
                console_fill(term, term->col, term->row, 
                                        info.srWindow.Right - term->col + 1);
                return;
            case 1:              
                // ESC[1K Clear from start of line to cursor
                console_fill(term, 0, term->row, term->col + 1);
                return;
            case 2:              
                // ESC[2K Clear whole line.
                console_fill(term, 0, term->row, term->width);
                return;
            default :
                return;
            }
        case 'L':
            // ESC[#L Insert # blank lines.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[L == ESC[1L
            if (es.argc != 1) return;
            console_scroll(term, 0, term->row, term->width-1, term->height-1, 
                                 0, term->row+es.argv[0]);
            console_fill(term, 0, term->row, term->width*es.argv[0]);
            return;
        case 'M':
            // ESC[#M Delete # line.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[M == ESC[1M
            if (es.argc != 1) return;
            if (es.argv[0] > term->height - term->row)
                es.argv[0] = term->height - term->row;
            console_scroll(term,  
                            0, term->row+es.argv[0], term->width-1, term->height-1, 
                            0, term->row);
            console_fill(term, 0, term->height-es.argv[0], term->width*es.argv[0]);
            return;
        case 'P':
            // ESC[#P Delete # characters.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[P == ESC[1P
            if (es.argc != 1) return;
            if (term->col + es.argv[0] > term->width - 1)
                es.argv[0] = term->width - term->col;
            console_scroll(term,
                    term->col+es.argv[0], term->row, term->width-1, term->row, 
                    term->col, term->row);
            console_fill(term, term->width-es.argv[0], term->row, es.argv[0]);
            return;
        case '@':
            // ESC[#@ Insert # blank characters.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[@ == ESC[1@
            if (es.argc != 1) return;
            if (term->col + es.argv[0] > term->width - 1)
                es.argv[0] = term->width - term->col;
            console_scroll(term,
                    term->col, term->row, term->width-1-es.argv[0], term->row, 
                    term->col+es.argv[0], term->row);
            console_fill(term, term->col, term->row, es.argv[0]);
            return;
        case 'A':
            // ESC[#A Moves cursor up # lines
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[A == ESC[1A
            if (es.argc != 1) return;
            console_set_pos(term, term->col, term->row - es.argv[0]);
            return;
        case 'B':
            // ESC[#B Moves cursor down # lines
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[B == ESC[1B
            if (es.argc != 1) return;
            console_set_pos(term, term->col, term->row + es.argv[0]);
            return;
        case 'C':
            // ESC[#C Moves cursor forward # spaces
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[C == ESC[1C
            if (es.argc != 1) return;
            console_set_pos(term, term->col + es.argv[0], term->row);
            return;
        case 'D':
            // ESC[#D Moves cursor back # spaces
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[D == ESC[1D
            if (es.argc != 1) return;
            console_set_pos(term, term->col - es.argv[0], term->row);
            return;
        case 'E':
            // ESC[#E Moves cursor down # lines, column 1.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[E == ESC[1E
            if (es.argc != 1) return;
            console_set_pos(term, 0, term->row + es.argv[0]);
            return;
        case 'F':
            // ESC[#F Moves cursor up # lines, column 1.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[F == ESC[1F
            if (es.argc != 1) return;
            console_set_pos(term, 0, term->row - es.argv[0]);
            return;
        case 'G':
            // ESC[#G Moves cursor column # in current row.
            if (es.argc == 0) es.argv[es.argc++] = 1;   // ESC[G == ESC[1G
            if (es.argc != 1) return;
            console_set_pos(term, es.argv[0] - 1, term->row);
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
            console_set_pos(term, es.argv[1]-1, es.argv[0]-1);
            return;
        case 's':
            // ESC[s Saves cursor position for recall later
            if (es.argc != 0) return;
            term->save_pos.X = term->col;
            term->save_pos.Y = term->row;
            return;
        case 'u':
            // ESC[u Return to saved cursor position
            if (es.argc != 0) return;
            SetConsoleCursorPosition(term->handle, term->save_pos);
            return;
        case 'r':
            // ESC[r set scroll region
            if (es.argc == 0) {
                // ESC[r == ESC[top;botr
                term->scroll_region.Top    = 0;
                term->scroll_region.Bottom = term->height - 1;
            }
            else if (es.argc == 2) {
                term->scroll_region.Top    = es.argv[0] - 1;
                term->scroll_region.Bottom = es.argv[1] - 1;
            }
            return;
        case 'S':
            // ESC[#S scroll up # lines
            if (es.argc != 1) return;
            console_scroll(term, 0, es.argv[0], term->width-1, term->height-1, 0, 0);
            console_fill(term, 0, term->scroll_region.Bottom, term->width*es.argv[0]);
            return;
        case 'T':
            // ESC[#T scroll down # lines
            if (es.argc != 1) return;
            console_scroll(term, 0, 0, term->width-1, term->height-es.argv[0]-1, 0, es.argv[0]);
            console_fill(term, 0, term->scroll_region.Top, term->width*es.argv[0]);
            return;
        case 't':
            //ESC[8;#;#;t resize terminal to # rows, # cols
            if (es.argc < 3) return;
            if (es.argv[0] != 8) return;
            COORD new_size;
            new_size.X = es.argv[2];
            new_size.Y = es.argv[1];
            SMALL_RECT new_screen;
            new_screen.Top = 0;
            new_screen.Left = 0;
            new_screen.Bottom = es.argv[1] - 1;
            new_screen.Right = es.argv[2] - 1;
            SetConsoleScreenBufferSize(term->handle, new_size);
            SetConsoleWindowInfo(term->handle, true, &new_screen);
            // do it twice, because one call can only make the console bigger 
            // and the other call can only make it smaller. plus I am lazy.
            SetConsoleScreenBufferSize(term->handle, new_size);
            SetConsoleWindowInfo(term->handle, true, &new_screen);
            return;
        }
    }
    else if (es.prefix == ']' && es.suffix == '\x07') {
        if (es.argc != 2) return;
        switch (es.argv[0]) {
            case 2:
                // ESC]2;%sBEL: set title
                SetConsoleTitle(es.args);
                break;
            case 255:
                // ANSIpipe-only: ESC]255;%sBEL: set terminal property
                // properties supported: ECHO, ICRNL, ONLCR
                // not thread-safe, so a bit unpredictable 
                // if you're using stdout and stderr at the same time.
                if (!wcscasecmp(es.args, L"ECHO"))
                    flags.echo = true;
                if (!wcscasecmp(es.args, L"ICRNL"))
                    flags.icrnl = true;
                else if (!wcscasecmp(es.args, L"ONLCR"))
                    flags.onlcr = true;
                break;
            case 254:
                // ANSIpipe-only: ESC]255;%sBEL: unset terminal property
                if (!wcscasecmp(es.args, L"ECHO"))
                    flags.echo = false;
                if (!wcscasecmp(es.args, L"ICRNL"))
                    flags.icrnl = false;
                else if (!wcscasecmp(es.args, L"ONLCR"))
                    flags.onlcr = false;
                break;
        }            
    }
}

// retrieve utf-8 and ansi sequences from standard input. 0 == success
int ansi_input(char *buffer, long buflen, long *count)
{
    // event buffer size: 
    // -  for utf8, buflen/4 is OK as one wchar is at most 4 chars of utf8
    // -  escape codes are at most 5 ascii-128 wchars; translate into 5 chars
    // so buflen/5 events should fit in buflen wchars and buflen utf8 chars.
    wchar_t wide_buffer[buflen];
    WSTR wstr = wstr_create_empty(wide_buffer, buflen);
    INPUT_RECORD events[buflen / 5];
    long ecount;
    if (!ReadConsoleInput(handle_cin, events, buflen / 5, &ecount))
        return 1;
    int i;
    wchar_t c;
    for (i = 0; i < ecount; ++i) {
        if (events[i].EventType == KEY_EVENT) {
            if (events[i].Event.KeyEvent.bKeyDown) {
                // insert ansi escape codes for arrow keys etc.
                switch (events[i].Event.KeyEvent.wVirtualKeyCode) {
                case VK_PRIOR:
                    wstr_write(&wstr, L"\x1b[5~", 4);
                    break;
                case VK_NEXT:
                    wstr_write(&wstr, L"\x1b[6~", 4);
                    break;
                case VK_END:
                    wstr_write(&wstr, L"\x1bOF", 3);
                    break;
                case VK_HOME:
                    wstr_write(&wstr, L"\x1bOH", 3);
                    break;
                case VK_LEFT:
                    wstr_write(&wstr, L"\x1b[D", 3);
                    break;
                case VK_UP:
                    wstr_write(&wstr, L"\x1b[A", 3);
                    break;
                case VK_RIGHT:
                    wstr_write(&wstr, L"\x1b[C", 3);
                    break;
                case VK_DOWN:
                    wstr_write(&wstr, L"\x1b[B", 3);
                    break;
                case VK_INSERT:
                    wstr_write(&wstr, L"\x1b[2~", 4);
                    break;
                case VK_DELETE:
                    wstr_write(&wstr, L"\x1b[3~", 4);
                    break;
                case VK_F1:
                    wstr_write(&wstr, L"\x1bOP", 3);
                    break;
                case VK_F2:
                    wstr_write(&wstr, L"\x1bOQ", 3);
                    break;
                case VK_F3:
                    wstr_write(&wstr, L"\x1bOR", 3);
                    break;
                case VK_F4:
                    wstr_write(&wstr, L"\x1bOS", 3);
                    break;
                case VK_F5:
                    wstr_write(&wstr, L"\x1b[15~", 5);
                    break;
                case VK_F6:
                    wstr_write(&wstr, L"\x1b[17~", 5);
                    break;
                case VK_F7:
                    wstr_write(&wstr, L"\x1b[18~", 5);
                    break;
                case VK_F8:
                    wstr_write(&wstr, L"\x1b[19~", 5);
                    break;
                case VK_F9:
                    wstr_write(&wstr, L"\x1b[20~", 5);
                    break;
                case VK_F10:
                    wstr_write(&wstr, L"\x1b[21~", 5);
                    break;
                case VK_F11:
                    wstr_write(&wstr, L"\x1b[23~", 5);
                    break;
                case VK_F12:
                    wstr_write(&wstr, L"\x1b[24~", 5);
                    break;
                default:
                    c = events[i].Event.KeyEvent.uChar.UnicodeChar;
                    wstr_write_char(&wstr, c);
                    if (flags.echo) {
                        if (c == L'\r')
                            printf("\n");
                        else
                            printf("%lc", c);
                    }
                }
                // overflow check
                if (wstr_write(&wstr, L"", 0) == NULL) {
                    fprintf(stderr, "ERROR: Input buffer overflow.\n");
                    return 1;
                }
                if (flags.icrnl) {
                    // replace last char CR -> LF
                    if (wstr.buffer[wstr.count-1] == L'\r') 
                        wstr.buffer[wstr.count-1] = L'\n';
                }
            }
        }
    }
    // find UTF8 string length    
    int length = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1, NULL, 0, NULL, NULL);
    // safety check
    if (length >= buflen) {
        fprintf(stderr, "ERROR: UTF-8 buffer overflow.\n");
        return 1;
    }
    // convert to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wide_buffer, -1, buffer, length, NULL, NULL);
    // exclude NUL terminator in reported num chars
    *count = length-1;
    return 0;
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
    p->term.handle = handle;
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
void parser_print(PARSER *p, char *s, int buflen)
{
    for (; buflen && *s; --buflen, ++s) {
        switch (p->state) {
        case 1:
            if (*s == '\x1b') p->state = 2;
            else pbuf_push(&(p->pbuf), p->term.concealed ? ' ' : *s);    
            break;
        case 2:
            if (*s == '\x1b');       // \e\e...\e == \e
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
                ansi_output(&(p->term), p->es);
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
                ansi_output(&(p->term), p->es);
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
                ansi_output(&(p->term), p->es);
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
                ansi_output(&(p->term), p->es);
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

// handles to named pipes
HANDLE cout_pipe;
HANDLE cin_pipe;
HANDLE cerr_pipe;

// Create named pipes for stdin, stdout and stderr
int pipes_create(long pid) 
{
    wchar_t name[ANSIPIPE_NAME_LEN];
    _snwprintf(name, ANSIPIPE_NAME_LEN, ANSIPIPE_POUT_FMT, pid);
    cout_pipe = CreateNamedPipe(name, PIPE_ACCESS_INBOUND, 
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, PIPES_BUFLEN, 
                        PIPES_BUFLEN, PIPES_TIMEOUT, NULL);
    if (INVALID_HANDLE_VALUE == cout_pipe) return 1;
    _snwprintf(name, ANSIPIPE_NAME_LEN, ANSIPIPE_PIN_FMT, pid);
    cin_pipe = CreateNamedPipe(name, PIPE_ACCESS_OUTBOUND, 
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, PIPES_BUFLEN, 
                        PIPES_BUFLEN, PIPES_TIMEOUT, NULL);
    if (INVALID_HANDLE_VALUE == cin_pipe) return 1;
    _snwprintf(name, ANSIPIPE_NAME_LEN, ANSIPIPE_PERR_FMT, pid);
    cerr_pipe = CreateNamedPipe(name, PIPE_ACCESS_INBOUND, 
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, PIPES_BUFLEN, 
                        PIPES_BUFLEN, PIPES_TIMEOUT, NULL);
    if (INVALID_HANDLE_VALUE == cerr_pipe) return 1;
    return 0;
}

// Thread function that handles incoming bytestreams to be output on stdout
void pipes_cout_thread(void *dummy) 
{
    // we're sending UTF-8 through these pipes
    char buffer[PIPES_BUFLEN];
    long count = 0;
    // this is 0 if redirected, -1 if not.
    // see http://stackoverflow.com/questions/2087775/how-do-i-detect-when-output-is-being-redirected
    fpos_t pos;
    fgetpos(stdout, &pos);
    PARSER p = {};
    parser_init(&p, handle_cout);
    ConnectNamedPipe(cout_pipe, NULL);
    while (ReadFile(cout_pipe, buffer, PIPES_BUFLEN-1, &count, NULL)) {
        buffer[count] = 0;
        if (pos) parser_print(&p, buffer, PIPES_BUFLEN);
        else printf("%s", buffer);
    }
}

// Thread function that handles incoming bytestreams to be outputed on stderr
void pipes_cerr_thread(void *dummy) 
{
    char buffer[PIPES_BUFLEN];
    long count = 0;
    fpos_t pos;
    fgetpos(stderr, &pos);
    PARSER p = {};
    parser_init(&p, handle_cerr);
    ConnectNamedPipe(cerr_pipe, NULL);
    while (ReadFile(cerr_pipe, buffer, PIPES_BUFLEN-1, &count, NULL)) {
        buffer[count] = 0;
        if (pos) parser_print(&p, buffer, PIPES_BUFLEN);
        else fprintf(stderr, "%s", buffer);
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
        if (ansi_input(buffer, PIPES_BUFLEN-1, &countr) != 0)
            break;
        if (!WriteFile(cin_pipe, buffer, countr, &countw, NULL))
            break;
    }
}

int pipes_start(PROCESS_INFORMATION pinfo)
{
    // spawn child process in suspended mode and create pipes
    if (pipes_create(pinfo.dwProcessId) != 0) {
        fprintf(stderr, "ERROR: Could not create named pipes.\n");
        return 1;
    }

    // set mode to accept keyboard event only
    SetConsoleMode(handle_cin, 0);

    // start the pipe threads
    _beginthread(pipes_cin_thread, 0, NULL);
    _beginthread(pipes_cout_thread, 0, NULL);
    _beginthread(pipes_cerr_thread, 0, NULL);
    return 0;
}

void pipes_close()    
{
    // Give pipes a chance to flush
    Sleep(100);
    CloseHandle(cout_pipe);
    CloseHandle(cerr_pipe);
    CloseHandle(cin_pipe);
}


// ============================================================================
// child process 
// ============================================================================

int proc_spawn(wchar_t *cmd_line, PROCESS_INFORMATION *pinfo)
{
    STARTUPINFO sinfo;
    memset(&sinfo, 0, sizeof(STARTUPINFO));
    sinfo.cb = sizeof(STARTUPINFO);
    if (!CreateProcess(NULL, cmd_line, NULL, NULL, FALSE, 
                       CREATE_SUSPENDED, NULL, NULL, &sinfo, pinfo)) {
        fprintf(stderr, "ERROR: Could not create process %S", cmd_line);
        return 1;
    }
    return 0;
}

void proc_join(PROCESS_INFORMATION pinfo, long *exit_code)
{
    // resume child process
    ResumeThread(pinfo.hThread);
    // wait for child process to exit and close pipes
    WaitForSingleObject(pinfo.hProcess, INFINITE);
    GetExitCodeProcess(pinfo.hProcess, exit_code);
}


// ============================================================================
// main function
// ============================================================================

// buffer for command-line arguments to child process
#define ARG_BUFLEN 2048

// self-call command line flag
#define STR_SELFCALL "ANSIPIPE_SELF_CALL"
#define WSTR_SELFCALL L"ANSIPIPE_SELF_CALL"

// create command line for child process
int build_command_line(int argc, char *argv[], wchar_t *buffer, long buflen)
{
    // start with empty string
    WSTR command_line = wstr_create_empty(buffer, buflen);
    // get full program name, exclude extension
    wchar_t module_name[MAX_PATH+1];
    GetModuleFileName(0, module_name, MAX_PATH);
    // not null-terminated on XP
    module_name[MAX_PATH] = 0;
    int module_len = wcslen(module_name);
    #ifdef ANSIPIPE_SINGLE
    wstr_write(&command_line, module_name, module_len);
    wstr_write(&command_line, L" ", 1);
    #else
    // only call X.EXE if we're named X.COM
    // if we're an EXE the first argument is the child executable, so skip this
    if (module_len > 4 && wcscasecmp(module_name + module_len - 4, L".exe")) {
        module_name[module_len-4] = 0;
        wstr_write(&command_line, module_name, module_len-4);
        wstr_write(&command_line, L".exe ", 5);
    }
    #endif
    // write all arguments to child command line
    int i = 0;
    for (i = 1; i < argc; ++i) {
        int length = MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, NULL, 0);
        wchar_t wide_buffer[length];
        // convert UTF-8 -> UTF-16. length includes NUL.
        MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, wide_buffer, length);
        wstr_write(&command_line, wide_buffer, length-1);
        wstr_write(&command_line, L" ", 1);
    }
    #ifdef ANSIPIPE_SINGLE
    wstr_write(&command_line, WSTR_SELFCALL, wcslen(WSTR_SELFCALL));
    #endif
    
    // if the buffer is full, our command line is not ok
    if (!wstr_write(&command_line, L"", 0)) {
        fprintf(stderr, "ERROR: Command line too long.\n");
        return 1;
    }
    return 0;
}

// launcher function
int ansipipe_launcher(int argc, char *argv[], long *exit_code)
{
    /* if this is a self-call, yield control to application main */
    #ifdef ANSIPIPE_SINGLE
    if (!strcmp(argv[argc-1], STR_SELFCALL))
        return 0;
    #endif    
    
    /* initialise globals */
    // stdio handles
    handle_cout = GetStdHandle(STD_OUTPUT_HANDLE);
    handle_cin = GetStdHandle(STD_INPUT_HANDLE);
    handle_cerr = GetStdHandle(STD_ERROR_HANDLE);

    /* save initial console state */
    CONSOLE_SCREEN_BUFFER_INFO save_console;
    long save_mode;
    GetConsoleScreenBufferInfo(handle_cout, &save_console);
    GetConsoleMode(handle_cin, &save_mode);

    /* open, run, and close */
    wchar_t cmd_line[ARG_BUFLEN];
    PROCESS_INFORMATION pinfo;    
    if (build_command_line(argc, argv, cmd_line, ARG_BUFLEN) == 0 &&
                proc_spawn(cmd_line, &pinfo) == 0 && 
                pipes_start(pinfo) == 0) {
        proc_join(pinfo, exit_code);
        pipes_close();
    }

    /* restore console state */
    SetConsoleMode(handle_cin, save_mode);
    SetConsoleTextAttribute(handle_cout, save_console.wAttributes);
    SetConsoleScreenBufferSize(handle_cout, save_console.dwSize);
    
    /* exit and signal to caller that we've run as launcher */
    return 1;
}

#ifndef ANSIPIPE_SINGLE
int main(int argc, char *argv[]) 
{
    long exit_code = 0;
    ansipipe_launcher(argc, argv, &exit_code);
    return exit_code;
}
#endif

#endif

