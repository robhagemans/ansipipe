/*
Win32::Console::ANSI
Copyright (c) 2003-2014 J-L Morel. All rights reserved.
This code has been relicensed in 2015 by kind permission from J-L Morel.

dualsubsystem
The author is identified on Google Code as gaber...@gmail.com, https://code.google.com/u/112032256711997475328/.
dualsubsystem is an update on a code written by Richard Eperjesi.

Licensed under the Expat MIT licence.
See LICENSE.md or http://opensource.org/licenses/mit-license.php
*/

#ifndef _WIN32

#ifndef ANSIPIPE_SINGLE
int main() {}
#endif

#else

#define UNICODE
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <unistd.h>
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

// size of pipe buffers
#define IO_BUFLEN 1024


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
// utf-8 buffer
// ============================================================================

// max length of utf-8 sequence is 4 bytes
#define U8BUF_SIZE 8

typedef struct {
    char buf[U8BUF_SIZE];
    int count;
    int size;
} U8BUF;

//  add a character in the buffer; return wchar if complete, NUL otherwise.
wchar_t u8buf_push(U8BUF *u8buf, unsigned char c)
{
    // skip NUL
    if (!c) return;
    u8buf->buf[u8buf->count++] = c;
    if (1 == u8buf->count) {
        // first byte determines sequence length
        if (c >= 0xf0) u8buf->size = 4;
        else if (c >= 0xe0) u8buf->size = 3;
        else if (c >= 0xc0) u8buf->size = 2;
        else u8buf->size = 1;
    }
    if (u8buf->count >= u8buf->size) {
        // leave space for NUL
        wchar_t wide_buffer[2] = L"";
        // utf8 sequences could be clipped if buffer is full, doesn't seem to happen
        MultiByteToWideChar(CP_UTF8, 0, u8buf->buf, u8buf->count, wide_buffer, 1);
        u8buf->count = 0;
        return wide_buffer[0];
    }
    return L'\0';
}



// ============================================================================
// windows console
// ============================================================================

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
    int end;
} TERM;

COORD onebyone = { 1, 1 };
COORD origin = { 0, 0 };

bool soft_suppress_stderr = false;

void console_put_char(TERM *term, wchar_t s)
{
    if (term->col >= term->width-1) {
        // do not advance cursor if we're on the last position of the
        // screen buffer, to avoid unwanted scrolling.
        SMALL_RECT dest = { term->col, term->row, term->col, term->row };
        CHAR_INFO ch;
        ch.Char.UnicodeChar = s;
        ch.Attributes = term->attr;
        WriteConsoleOutput(term->handle, &ch, onebyone, origin, &dest);
    }
    else {
        long written;
        WriteConsole(term->handle, &s, 1, &written, NULL);
    }
}

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
                console_fill(term, term->col, term->row, term->end - term->col + 1);
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
                // special property: SUPPSTERR - suppress stderr
                if (!wcscasecmp(es.args, L"ECHO"))
                    flags.echo = true;
                else if (!wcscasecmp(es.args, L"ICRNL"))
                    flags.icrnl = true;
                else if (!wcscasecmp(es.args, L"ONLCR"))
                    flags.onlcr = true;
                else if (!wcscasecmp(es.args, L"SUPPSTDERR"))
                    soft_suppress_stderr = true;
                break;
            case 254:
                // ANSIpipe-only: ESC]254;%sBEL: unset terminal property
                if (!wcscasecmp(es.args, L"ECHO"))
                    flags.echo = false;
                else if (!wcscasecmp(es.args, L"ICRNL"))
                    flags.icrnl = false;
                else if (!wcscasecmp(es.args, L"ONLCR"))
                    flags.onlcr = false;
                else if (!wcscasecmp(es.args, L"SUPPSTDERR"))
                    soft_suppress_stderr = false;
                break;
        }
    }
}

// retrieve utf-8 and ansi sequences from standard input. 0 == success
// length of buffer must be IO_BUFLEN
int ansi_input(char *buffer, long *count)
{
    // event buffer size:
    // -  for utf8, buflen/4 is OK as one wchar is at most 4 chars of utf8
    // -  escape codes are at most 5 ascii-128 wchars; translate into 5 chars
    // so buflen/5 events should fit in buflen wchars and buflen utf8 chars.
    // plus one char for NUL.
//    long buflen = IO_BUFLEN;
//    long events_len = (buflen-1)/5;
    wchar_t wide_buffer[IO_BUFLEN];
    WSTR wstr = wstr_create_empty(wide_buffer, IO_BUFLEN);
    INPUT_RECORD events[(IO_BUFLEN-1)/5];
    long ecount;
    if (!ReadConsoleInput(handle_cin, events, (IO_BUFLEN-1)/5, &ecount))
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
    if (length >= IO_BUFLEN) {
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
    SEQUENCE es;
    TERM term;
    U8BUF pbuf;
    int state;
} PARSER;

// initialise a new ansi sequence parser
void parser_init(PARSER *p, HANDLE handle)
{
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
        // retrieve current positions and sizes
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(p->term.handle, &info);
        p->term.col = info.dwCursorPosition.X;
        p->term.row = info.dwCursorPosition.Y;
        p->term.width = info.dwSize.X;
        p->term.height = info.dwSize.Y;
        p->term.attr = info.wAttributes;
        p->term.end = info.srWindow.Right;
        switch (p->state) {
        case 1:
            if (*s == '\x1b') {
                p->state = 2;
            }
            else {
                wchar_t wc = u8buf_push(&p->pbuf, p->term.concealed ? ' ' : *s);
                if (wc) console_put_char(&p->term, wc);
                if (flags.onlcr && wc == L'\r')
                        console_put_char(&p->term, L'\n');
            }
            break;
        case 2:
            if (*s == '\x1b');       // \e\e...\e == \e
            else if (*s == '[') {
                p->es.prefix = *s;
                p->es.prefix2 = 0;
                p->state = 3;
            }
            else if (*s == ']') {
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
}


// ============================================================================
// named pipes
// ============================================================================

// pipe globals
#define PIPES_TIMEOUT 1000

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
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, IO_BUFLEN,
                        IO_BUFLEN, PIPES_TIMEOUT, NULL);
    if (INVALID_HANDLE_VALUE == cout_pipe) return 1;
    _snwprintf(name, ANSIPIPE_NAME_LEN, ANSIPIPE_PIN_FMT, pid);
    cin_pipe = CreateNamedPipe(name, PIPE_ACCESS_OUTBOUND,
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, IO_BUFLEN,
                        IO_BUFLEN, PIPES_TIMEOUT, NULL);
    if (INVALID_HANDLE_VALUE == cin_pipe) return 1;
    _snwprintf(name, ANSIPIPE_NAME_LEN, ANSIPIPE_PERR_FMT, pid);
    cerr_pipe = CreateNamedPipe(name, PIPE_ACCESS_INBOUND,
                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, IO_BUFLEN,
                        IO_BUFLEN, PIPES_TIMEOUT, NULL);
    if (INVALID_HANDLE_VALUE == cerr_pipe) return 1;
    return 0;
}

// handle incoming pipes to be output on stdout or stderr
void pipes_output_thread(void *dummy)
{
    // check if we're on a console or being redirected
    // see http://stackoverflow.com/questions/1169591/check-if-output-is-redirected
    long dummy_mode;
    int is_console_cout = GetConsoleMode(handle_cout, &dummy_mode);
    int is_console_cerr = GetConsoleMode(handle_cerr, &dummy_mode);
    // connect to named pipes
    ConnectNamedPipe(cout_pipe, NULL);
    ConnectNamedPipe(cerr_pipe, NULL);
    // prepare parsers
    PARSER pout = { 0 };
    PARSER perr = { 0 };
    parser_init(&pout, handle_cout);
    parser_init(&perr, handle_cerr);
    // read buffer
    char buffer[IO_BUFLEN];
    long count = 0;
    bool cout_alive = true;
    bool cerr_alive = true;
    // read from stdout and stderr alternatively until exhausted
    // in one thread to avoid them racing for console output
    while (cout_alive && cerr_alive) {
        // read from stdout if not closed and available
        if (cout_alive) {
            for(;;) {
                long len = 0;
                PeekNamedPipe(cout_pipe, NULL, 0, NULL, &len, NULL);
                if (!len) {
                    // microsleep to allow the pipe to fill
                    usleep(1);
                    break;
                }
                cout_alive = ReadFile(cout_pipe, buffer, IO_BUFLEN-1, &count, NULL);
                if (cout_alive) {
                    buffer[count] = 0;
                    if (is_console_cout) parser_print(&pout, buffer, IO_BUFLEN);
                    else printf("%s", buffer);
                }
            }
        }
        // read from stderr if not closed and available
        if (cerr_alive) {
            for(;;) {
                long len = 0;
                PeekNamedPipe(cerr_pipe, NULL, 0, NULL, &len, NULL);
                if (!len) {
                    // microsleep to allow the pipe to fill
                    usleep(1);
                    break;
                }
                cerr_alive = ReadFile(cerr_pipe, buffer, IO_BUFLEN-1, &count, NULL);
                if (cerr_alive) {
                    buffer[count] = 0;
                    if (is_console_cerr) parser_print(&perr, buffer, IO_BUFLEN);
                    else fprintf(stderr, "%s", buffer);
                }
            }
        }
    }
}

// handle input from stdin and send to outgoing pipe
void pipes_input_thread(void *dummy)
{
    // see http://stackoverflow.com/questions/1169591/check-if-output-is-redirected
    long dummy_mode;
    int is_console = GetConsoleMode(handle_cin, &dummy_mode);
    // we're receiving UTF-8 through these pipes
    char buffer[IO_BUFLEN];
    long countr = 0;
    long countw = 0;
    ConnectNamedPipe(cin_pipe, NULL);
    for(;;) {
        if (is_console) {
            if (ansi_input(buffer, &countr) != 0)
                break;
        }
        else {
            // read directly from redirected stdin
            if (!fgets(buffer, IO_BUFLEN, stdin))
                break;
            // fgets returns null-terminated string
            countr = strlen(buffer);
        }
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
    _beginthread(pipes_input_thread, 0, NULL);
    _beginthread(pipes_output_thread, 0, NULL);
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

// buffer for command-line arguments to child process. Win32 max is 8K
#define ARG_BUFLEN 8192

// self-call command line flag
#define STR_SELFCALL "ANSIPIPE_SELF_CALL"
#define WSTR_SELFCALL L"ANSIPIPE_SELF_CALL"

// create command line for child process
int build_command_line(wchar_t *buffer, long buflen)
{
    // start with empty string
    WSTR command_line = wstr_create_empty(buffer, buflen);
    // get full program name, exclude extension
    wchar_t module_name[MAX_PATH+1];
    GetModuleFileName(0, module_name, MAX_PATH);
    // not null-terminated on XP
    module_name[MAX_PATH] = 0;
    int module_len = wcslen(module_name);
    // write name of child executable to command line in quotes
    #ifdef ANSIPIPE_SINGLE
    wstr_write(&command_line, L"""", 1);
    wstr_write(&command_line, module_name, module_len);
    wstr_write(&command_line, L""" ", 2);
    #else
    // call X.EXE if we're named X.COM
    // if we're X.EXE the first argument already is the child executable, so skip this step
    if (module_len > 4 && wcscasecmp(module_name + module_len - 4, L".exe")) {
        module_name[module_len-4] = 0;
        wstr_write(&command_line, L"""", 1);
        wstr_write(&command_line, module_name, module_len-4);
        wstr_write(&command_line, L".exe"" ", 6);
    }
    #endif
    // input command line
    wchar_t *orig_command_line;
    orig_command_line = GetCommandLineW();
    // find length of first argument including quotes, if any
    int argc;
    wchar_t **argv = CommandLineToArgvW(orig_command_line, &argc);
    if (!argv) {
        fprintf(stderr, "ERROR: Could not parse command line.");
    }
    int skip_len = wcslen(argv[0]) + 1;
    if (orig_command_line[0] == L'"') {
        // module name is quoted in command line: remove quotes too
        skip_len += 2;
    }
    // copy original command line excluding module name into child command line
    wstr_write(&command_line, orig_command_line+skip_len, wcslen(orig_command_line)-skip_len);

    #ifdef ANSIPIPE_SINGLE
    // write the self-call flag
    wstr_write(&command_line, L" ", 1);
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
    if (build_command_line(cmd_line, ARG_BUFLEN) == 0 &&
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
