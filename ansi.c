//#define PERL_NO_GET_CONTEXT     /* we want efficiency */
//#include "EXTERN.h"
//#include "perl.h"
//#include "XSUB.h"

//#include "ppport.h"

#define _WIN32_WINNT 0x0500
#include <windows.h>
//#include <ImageHlp.h>
//#include <tlhelp32.h>

//#ifndef MIIM_BITMAP
//#error your SDK is too old... (see the README file in the distro)
//#endif

// ========== Auxiliary debug function
// See DebugView from SysInternals:
// http://technet.microsoft.com/fr-fr/sysinternals/bb896647(en-us).aspx

#define MYDEBUG 0

#if (MYDEBUG > 0)
void DEBUGSTR( char * szFormat, ...) {  // sort of OutputDebugStringf
  char szBuffer[1024];
  va_list pArgList;
  va_start(pArgList, szFormat);
  _vsnprintf(szBuffer, sizeof(szBuffer), szFormat, pArgList);
  va_end(pArgList);
  OutputDebugString(szBuffer);
}
#else
#define DEBUGSTR //DEBUGSTR
#endif

// ========== Global variables and constants

// Macro for adding pointers/DWORDs together without C arithmetic interfering
#define MakePtr( cast, ptr, addValue ) (cast)( (DWORD_PTR)(ptr)+(DWORD)(addValue))

//HINSTANCE hDllInstance;         // Dll instance handle
//HWND hConWnd;                   // Console window handle
HANDLE hConOut;                 // handle to CONOUT$
//BOOL bIsWin9x;
//HMENU hSysMenu;                 // handle to console system menu
MENUITEMINFO CloseMenuItemInfo; // close menu item
int CloseMenuItemPos = -1;      // close menu item position
                                // prototype for SetConsoleDisplayMode()
typedef BOOL (WINAPI *SETCONDISPMODE)(HANDLE, DWORD, PCOORD);
SETCONDISPMODE pfnSetConDispMode;

#define ESC     '\x1B'          // ESCape character
#define LF      '\x0A'          // Line Feed

#define MAX_TITLE_SIZE 1024     // max title string console size

#define MAX_ARG 16              // max number of args in an escape sequence
int state;                      // automata state
char prefix;                    // escape sequence prefix ( '[' or '(' );
char prefix2;			              // secondary prefix ( '?' );
char suffix;                    // escape sequence suffix
int es_argc;                    // escape sequence args count
int es_argv[MAX_ARG];           // escape sequence args

// color constants

#define FOREGROUND_BLACK 0
#define FOREGROUND_WHITE FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE

#define BACKGROUND_BLACK 0
#define BACKGROUND_WHITE BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE

WORD foregroundcolor[16] = {
  FOREGROUND_BLACK,                                       // black foreground
  FOREGROUND_RED,                                         // red foreground
  FOREGROUND_GREEN,                                       // green foreground
  FOREGROUND_RED|FOREGROUND_GREEN,                        // yellow foreground
  FOREGROUND_BLUE,                                        // blue foreground
  FOREGROUND_BLUE|FOREGROUND_RED,                         // magenta foreground
  FOREGROUND_BLUE|FOREGROUND_GREEN,                       // cyan foreground
  FOREGROUND_WHITE,                                       // white foreground
  FOREGROUND_BLACK|FOREGROUND_INTENSITY,                  // black foreground bright
  FOREGROUND_RED|FOREGROUND_INTENSITY,                    // red foreground bright
  FOREGROUND_GREEN|FOREGROUND_INTENSITY,                  // green foreground bright
  FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,   // yellow foreground bright
  FOREGROUND_BLUE|FOREGROUND_INTENSITY ,                  // blue foreground bright
  FOREGROUND_BLUE|FOREGROUND_RED|FOREGROUND_INTENSITY,    // magenta foreground bright
  FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_INTENSITY,  // cyan foreground bright
  FOREGROUND_WHITE|FOREGROUND_INTENSITY                   // gray foreground bright
  };

WORD backgroundcolor[16] = {
  BACKGROUND_BLACK,                                       // black background
  BACKGROUND_RED,                                         // red background
  BACKGROUND_GREEN,                                       // green background
  BACKGROUND_RED|BACKGROUND_GREEN,                        // yellow background
  BACKGROUND_BLUE,                                        // blue background
  BACKGROUND_BLUE|BACKGROUND_RED,                         // magenta background
  BACKGROUND_BLUE|BACKGROUND_GREEN,                       // cyan background
  BACKGROUND_WHITE,                                       // white background
  BACKGROUND_BLACK|BACKGROUND_INTENSITY,                  // black background bright
  BACKGROUND_RED|BACKGROUND_INTENSITY,                    // red background bright
  BACKGROUND_GREEN|BACKGROUND_INTENSITY,                  // green background bright
  BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_INTENSITY,   // yellow background bright
  BACKGROUND_BLUE|BACKGROUND_INTENSITY,                   // blue background bright
  BACKGROUND_BLUE|BACKGROUND_RED|BACKGROUND_INTENSITY,    // magenta background bright
  BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_INTENSITY,  // cyan background bright
  BACKGROUND_WHITE|BACKGROUND_INTENSITY                   // white background bright
};

// Table to convert the color order of the console in the ANSI order.
WORD conversion[16] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};

// screen attributes
WORD foreground = FOREGROUND_WHITE;
WORD background = BACKGROUND_BLACK;
WORD foreground_default = FOREGROUND_WHITE;
WORD background_default = BACKGROUND_BLACK;
WORD bold       = 0;
WORD underline  = 0;
WORD rvideo     = 0;
WORD concealed  = 0;
WORD conversion_enabled = 1;  // enabled by default ANSI(Win) --> OEM(Dos)
UINT Cp_In  = CP_ACP;         // default script codepage (ANSI)
UINT Cp_Out = CP_OEMCP;       // default ouput codepage  (OEM)
UINT SaveCP;

// saved cursor position
COORD SavePos = {0, 0};


// ========== Print Buffer functions

#define BUFFER_SIZE 256

int nCharInBuffer = 0;
char  ChBuffer[BUFFER_SIZE];
WCHAR WcBuffer[BUFFER_SIZE];

//-----------------------------------------------------------------------------
//   FlushBuffer()
// Converts the buffer from ANSI to OEM and write it in the console.
//-----------------------------------------------------------------------------

void FlushBuffer( )
{
  DWORD nWritten;
  if (nCharInBuffer <= 0) return;
  if ( conversion_enabled ) {
    MultiByteToWideChar(Cp_In, 0, ChBuffer, nCharInBuffer, WcBuffer, BUFFER_SIZE);
    WideCharToMultiByte(Cp_Out, 0, WcBuffer, nCharInBuffer, ChBuffer, BUFFER_SIZE, NULL, NULL);
  }
  WriteConsole(hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL);
  nCharInBuffer = 0;
}

//-----------------------------------------------------------------------------
//   PushBuffer( char c)
// Adds a character in the buffer and flushes the buffer if it is full
//-----------------------------------------------------------------------------

void PushBuffer( char c)
{
  ChBuffer[nCharInBuffer++] = concealed ? ' ' : c;
  if (nCharInBuffer >= BUFFER_SIZE) {
    FlushBuffer();
    DEBUGSTR("flush");
  }
}

// ========== Print functions

//-----------------------------------------------------------------------------
//   InterpretEscSeq( )
// Interprets the last escape sequence scanned by ParseAndPrintString
//   prefix             escape sequence prefix
//   es_argc            escape sequence args count
//   es_argv[]          escape sequence args array
//   suffix             escape sequence suffix
//
// for instance, with \e[33;45;1m we have
// prefix = '[',
// es_argc = 3, es_argv[0] = 33, es_argv[1] = 45, es_argv[2] = 1
// suffix = 'm'
//-----------------------------------------------------------------------------

void InterpretEscSeq( )
{
  int i;
  WORD attribut;
  CONSOLE_SCREEN_BUFFER_INFO Info;
  CONSOLE_CURSOR_INFO CursInfo;
  DWORD len, NumberOfCharsWritten;
  COORD Pos;
  SMALL_RECT Rect;
  CHAR_INFO CharInfo;

  if (prefix == '[') {
    if (prefix2 == '?' && (suffix == 'h' || suffix == 'l')) {
      if (es_argc == 1 && es_argv[0] == 25) {
        GetConsoleCursorInfo( hConOut, &CursInfo );
        CursInfo.bVisible = (suffix == 'h');
        SetConsoleCursorInfo( hConOut, &CursInfo );
        return;
      }
    }
    // Ignore any other \e[? sequences.
    if (prefix2 != 0) return;

    GetConsoleScreenBufferInfo(hConOut, &Info);
    switch (suffix) {
      case 'm':
        if ( es_argc == 0 ) es_argv[es_argc++] = 0;
        for(i=0; i<es_argc; i++) {
          switch (es_argv[i]) {
            case 0 :
              foreground = foreground_default;
              background = background_default;
              DEBUGSTR("resetting foreground to  = 0x%.8x", foreground);
              DEBUGSTR("resetting background to  = 0x%.8x", background);
              bold = 0;
              underline = 0;
              rvideo = 0;
              concealed = 0;
              break;
            case 1 :
              bold = 1;
              break;
            case 21 :
              bold = 0;
              break;
            case 4 :
              underline = 1;
              break;
            case 24 :
              underline = 0;
              break;
            case 7 :
              rvideo = 1;
              break;
            case 27 :
              rvideo = 0;
              break;
            case 8 :
              concealed = 1;
              break;
            case 28 :
              concealed = 0;
              break;
          }
          if ( (30 <= es_argv[i]) && (es_argv[i] <= 37) ) {
            foreground = es_argv[i]-30;
            DEBUGSTR("setting foreground to = 0x%.8x", foreground);
          }
          if ( (40 <= es_argv[i]) && (es_argv[i] <= 47) ) {
            DEBUGSTR("setting background to = 0x%.8x", background);
            background = es_argv[i]-40;
          }
        }
        if (rvideo) attribut = foregroundcolor[background] | backgroundcolor[foreground];
        else attribut = foregroundcolor[foreground] | backgroundcolor[background];
        if (bold) attribut |= FOREGROUND_INTENSITY;
        if (underline) attribut |= BACKGROUND_INTENSITY;
        DEBUGSTR("set console color to = 0x%.8x", attribut);
        SetConsoleTextAttribute(hConOut, attribut);
        return;

      case 'J':
        if ( es_argc == 0 ) es_argv[es_argc++] = 0;   // ESC[J == ESC[0J
        if ( es_argc != 1 ) return;
        switch (es_argv[0]) {
          case 0 :              // ESC[0J erase from cursor to end of display
            len = (Info.dwSize.Y-Info.dwCursorPosition.Y-1)
                  *Info.dwSize.X+Info.dwSize.X-Info.dwCursorPosition.X-1;
            FillConsoleOutputCharacter(
              hConOut,
              ' ',
              len,
              Info.dwCursorPosition,
              &NumberOfCharsWritten);

            FillConsoleOutputAttribute(
              hConOut,
              Info.wAttributes,
              len,
              Info.dwCursorPosition,
              &NumberOfCharsWritten);
            return;

          case 1 :              // ESC[1J erase from start to cursor.
            Pos.X = 0;
            Pos.Y = 0;
            len = Info.dwCursorPosition.Y*Info.dwSize.X+Info.dwCursorPosition.X+1;
            FillConsoleOutputCharacter(
              hConOut,
              ' ',
              len,
              Pos,
              &NumberOfCharsWritten);

            FillConsoleOutputAttribute(
              hConOut,
              Info.wAttributes,
              len,
              Pos,
              &NumberOfCharsWritten);
            return;

          case 2 :              // ESC[2J Clear screen and home cursor
            Pos.X = 0;
            Pos.Y = 0;
            len = Info.dwSize.X*Info.dwSize.Y;
            FillConsoleOutputCharacter(
              hConOut,
              ' ',
              len,
              Pos,
              &NumberOfCharsWritten);
            FillConsoleOutputAttribute(
              hConOut,
              Info.wAttributes,
              len,
              Pos,
              &NumberOfCharsWritten);
            SetConsoleCursorPosition(hConOut, Pos);
            return;

          default :
            return;
        }

      case 'K' :
        if ( es_argc == 0 ) es_argv[es_argc++] = 0;   // ESC[K == ESC[0K
        if ( es_argc != 1 ) return;
        switch (es_argv[0]) {
          case 0 :              // ESC[0K Clear to end of line
            len = Info.srWindow.Right-Info.dwCursorPosition.X+1;
            FillConsoleOutputCharacter(
              hConOut,
              ' ',
              len,
              Info.dwCursorPosition,
              &NumberOfCharsWritten);

            FillConsoleOutputAttribute(
              hConOut,
              Info.wAttributes,
              len,
              Info.dwCursorPosition,
              &NumberOfCharsWritten);
            return;

          case 1 :              // ESC[1K Clear from start of line to cursor
            Pos.X = 0;
            Pos.Y = Info.dwCursorPosition.Y;
            FillConsoleOutputCharacter(
              hConOut,
              ' ',
              Info.dwCursorPosition.X+1,
              Pos,
              &NumberOfCharsWritten);

            FillConsoleOutputAttribute(
              hConOut,
              Info.wAttributes,
              Info.dwCursorPosition.X+1,
              Pos,
              &NumberOfCharsWritten);
            return;

          case 2 :              // ESC[2K Clear whole line.
            Pos.X = 0;
            Pos.Y = Info.dwCursorPosition.Y;
            FillConsoleOutputCharacter(
              hConOut,
              ' ',
              Info.dwSize.X,
              Pos,
              &NumberOfCharsWritten);
            FillConsoleOutputAttribute(
              hConOut,
              Info.wAttributes,
              Info.dwSize.X,
              Pos,
              &NumberOfCharsWritten);
            return;

          default :
            return;
        }

      case 'L' :                                    // ESC[#L Insert # blank lines.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[L == ESC[1L
        if ( es_argc != 1 ) return;
        Rect.Left   = 0;
        Rect.Top    = Info.dwCursorPosition.Y;
        Rect.Right  = Info.dwSize.X-1;
        Rect.Bottom = Info.dwSize.Y-1;
        Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y+es_argv[0];
        CharInfo.Char.AsciiChar = ' ';
        CharInfo.Attributes = Info.wAttributes;
        ScrollConsoleScreenBuffer(
          hConOut,
          &Rect,
          NULL,
          Pos,
          &CharInfo);
        Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y;
        FillConsoleOutputCharacter(
          hConOut,
          ' ',
          Info.dwSize.X*es_argv[0],
          Pos,
          &NumberOfCharsWritten);
        FillConsoleOutputAttribute(
          hConOut,
          Info.wAttributes,
          Info.dwSize.X*es_argv[0],
          Pos,
          &NumberOfCharsWritten);
        return;

      case 'M' :                                      // ESC[#M Delete # line.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[M == ESC[1M
        if ( es_argc != 1 ) return;
        if ( es_argv[0] > Info.dwSize.Y - Info.dwCursorPosition.Y )
          es_argv[0] = Info.dwSize.Y - Info.dwCursorPosition.Y;
        Rect.Left   = 0;
        Rect.Top    = Info.dwCursorPosition.Y+es_argv[0];
        Rect.Right  = Info.dwSize.X-1;
        Rect.Bottom = Info.dwSize.Y-1;
        Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y;
        CharInfo.Char.AsciiChar = ' ';
        CharInfo.Attributes = Info.wAttributes;
        ScrollConsoleScreenBuffer(
          hConOut,
          &Rect,
          NULL,
          Pos,
          &CharInfo);
        Pos.Y = Info.dwSize.Y - es_argv[0];
        FillConsoleOutputCharacter(
          hConOut,
          ' ',
          Info.dwSize.X * es_argv[0],
          Pos,
          &NumberOfCharsWritten);
        FillConsoleOutputAttribute(
          hConOut,
          Info.wAttributes,
          Info.dwSize.X * es_argv[0],
          Pos,
          &NumberOfCharsWritten);
        return;

      case 'P' :                                      // ESC[#P Delete # characters.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[P == ESC[1P
        if ( es_argc != 1 ) return;
        if (Info.dwCursorPosition.X + es_argv[0] > Info.dwSize.X - 1)
              es_argv[0] = Info.dwSize.X - Info.dwCursorPosition.X;

        Rect.Left   = Info.dwCursorPosition.X + es_argv[0];
        Rect.Top    = Info.dwCursorPosition.Y;
        Rect.Right  = Info.dwSize.X-1;
        Rect.Bottom = Info.dwCursorPosition.Y;
        CharInfo.Char.AsciiChar = ' ';
        CharInfo.Attributes = Info.wAttributes;
        ScrollConsoleScreenBuffer(
          hConOut,
          &Rect,
          NULL,
          Info.dwCursorPosition,
          &CharInfo);
        Pos.X = Info.dwSize.X - es_argv[0];
        Pos.Y = Info.dwCursorPosition.Y;
        FillConsoleOutputCharacter(
          hConOut,
          ' ',
          es_argv[0],
          Pos,
          &NumberOfCharsWritten);
        return;

      case '@' :                                      // ESC[#@ Insert # blank characters.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[@ == ESC[1@
        if ( es_argc != 1 ) return;
        if (Info.dwCursorPosition.X + es_argv[0] > Info.dwSize.X - 1)
          es_argv[0] = Info.dwSize.X - Info.dwCursorPosition.X;
        Rect.Left   = Info.dwCursorPosition.X;
        Rect.Top    = Info.dwCursorPosition.Y;
        Rect.Right  = Info.dwSize.X-1-es_argv[0];
        Rect.Bottom = Info.dwCursorPosition.Y;
        Pos.X = Info.dwCursorPosition.X+es_argv[0];
        Pos.Y = Info.dwCursorPosition.Y;
        CharInfo.Char.AsciiChar = ' ';
        CharInfo.Attributes = Info.wAttributes;
        ScrollConsoleScreenBuffer(
          hConOut,
          &Rect,
          NULL,
          Pos,
          &CharInfo);
        FillConsoleOutputCharacter(
          hConOut,
          ' ',
          es_argv[0],
          Info.dwCursorPosition,
          &NumberOfCharsWritten);
        FillConsoleOutputAttribute(
          hConOut,
          Info.wAttributes,
          es_argv[0],
          Info.dwCursorPosition,
          &NumberOfCharsWritten);
        return;

      case 'A' :                                      // ESC[#A Moves cursor up # lines
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[A == ESC[1A
        if ( es_argc != 1 ) return;
        Pos.X = Info.dwCursorPosition.X;
        Pos.Y = Info.dwCursorPosition.Y-es_argv[0];
        if (Pos.Y < 0) Pos.Y = 0;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'B' :                                      // ESC[#B Moves cursor down # lines
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[B == ESC[1B
        if ( es_argc != 1 ) return;
        Pos.X = Info.dwCursorPosition.X;
        Pos.Y = Info.dwCursorPosition.Y+es_argv[0];
        if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y-1;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'C' :                                      // ESC[#C Moves cursor forward # spaces
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[C == ESC[1C
        if ( es_argc != 1 ) return;
        Pos.X = Info.dwCursorPosition.X+es_argv[0];
        if ( Pos.X >= Info.dwSize.X ) Pos.X = Info.dwSize.X-1;
        Pos.Y = Info.dwCursorPosition.Y;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'D' :                                      // ESC[#D Moves cursor back # spaces
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[D == ESC[1D
        if ( es_argc != 1 ) return;
        Pos.X = Info.dwCursorPosition.X-es_argv[0];
        if ( Pos.X < 0 ) Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'E' :                               // ESC[#E Moves cursor down # lines, column 1.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[E == ESC[1E
        if ( es_argc != 1 ) return;
        Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y+es_argv[0];
        if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y-1;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'F' :                               // ESC[#F Moves cursor up # lines, column 1.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[F == ESC[1F
        if ( es_argc != 1 ) return;
        Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y-es_argv[0];
        if ( Pos.Y < 0 ) Pos.Y = 0;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'G' :                               // ESC[#G Moves cursor column # in current row.
        if ( es_argc == 0 ) es_argv[es_argc++] = 1;   // ESC[G == ESC[1G
        if ( es_argc != 1 ) return;
        Pos.X = es_argv[0] - 1;
        if ( Pos.X >= Info.dwSize.X ) Pos.X = Info.dwSize.X-1;
        if ( Pos.X < 0) Pos.X = 0;
        Pos.Y = Info.dwCursorPosition.Y;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 'f' :
      case 'H' :                               // ESC[#;#H or ESC[#;#f Moves cursor to line #, column #
        if ( es_argc == 0 ) {
          es_argv[es_argc++] = 1;   // ESC[G == ESC[1;1G
          es_argv[es_argc++] = 1;
        }
        if ( es_argc == 1 ) {
          es_argv[es_argc++] = 1;   // ESC[nG == ESC[n;1G
        }
        if ( es_argc > 2 ) return;
        Pos.X = es_argv[1] - 1;
        if ( Pos.X < 0) Pos.X = 0;
        if ( Pos.X >= Info.dwSize.X ) Pos.X = Info.dwSize.X-1;
        Pos.Y = es_argv[0] - 1;
        if ( Pos.Y < 0) Pos.Y = 0;
        if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y-1;
        SetConsoleCursorPosition(hConOut, Pos);
        return;

      case 's' :                               // ESC[s Saves cursor position for recall later
        if ( es_argc != 0 ) return;
        SavePos.X = Info.dwCursorPosition.X;
        SavePos.Y = Info.dwCursorPosition.Y;
        return;

      case 'u' :                               // ESC[u Return to saved cursor position
        if ( es_argc != 0 ) return;
        SetConsoleCursorPosition(hConOut, SavePos);
        return;

      default :
        return;

    }
  }
  else if (prefix == '(') {
    switch (suffix) {
      case 'U' :                                // ESC(U no mapping
        if ( es_argc != 0 ) return;
        FlushBuffer();
        conversion_enabled = 0;
        return;

      case 'K' :                                // ESC(K mapping if it exist
        if ( es_argc != 0 ) return;
        FlushBuffer();
        SetConsoleOutputCP(SaveCP);
        Cp_Out = CP_OEMCP;
        conversion_enabled = 1;
        return;

      case 'X' :                                // ESC(#X codepage **EXPERIMENTAL**
        if ( es_argc != 1 ) return;
        FlushBuffer();
        SetConsoleOutputCP(es_argv[0]);
        conversion_enabled = 0;
        return;
    }
  }
  else {

  }
}

HANDLE hCurrentDev = 0;     // handle to the current device

//-----------------------------------------------------------------------------
//   ParseAndPrintString(hDev, lpBuffer, nNumberOfBytesToWrite)
// Parses the string lpBuffer, interprets the escapes sequences and prints the
// characters in the device hDev (console).
// The lexer is a four states automata.
// If the number of arguments es_argc > MAX_ARG, only the MAX_ARG-1 firsts and
// the last arguments are processed (no es_argv[] overflow).
//-----------------------------------------------------------------------------

BOOL
ParseAndPrintString(HANDLE hDev,
                    LPCVOID lpBuffer,
                    DWORD nNumberOfBytesToWrite,
                    LPDWORD lpNumberOfBytesWritten
                    )
{
  DWORD i;
  char * s;
  
  //dTHX;
  if (hDev != hCurrentDev) {
    hCurrentDev = hDev;
    state = 1;            // reinit if device have changed
  }
  for(i=nNumberOfBytesToWrite, s=(char *)lpBuffer; i>0; i--, s++) {
    if (state==1) {
      // Under Win9x, at each new line, the console fills the end of the line with
      // the default attribute. We correct this behavior here.

/*      if ( bIsWin9x && (*s == LF) ) {
        CONSOLE_SCREEN_BUFFER_INFO Info;
        COORD Pos = {0, 0};
        SMALL_RECT Rect;
        CHAR_INFO CharInfo;

        FlushBuffer();
        GetConsoleScreenBufferInfo(hConOut, &Info);
        Info.dwCursorPosition.Y++;
        if ( Info.dwCursorPosition.Y < Info.dwSize.Y ){
          Info.dwCursorPosition.X = 0;
          SetConsoleCursorPosition(hConOut, Info.dwCursorPosition);
        }
        else {
          Rect.Left   = 0;
          Rect.Top    = 1;
          Rect.Right  = Info.dwSize.X-1;
          Rect.Bottom = Info.dwSize.Y-1;
          CharInfo.Char.AsciiChar = ' ';
          CharInfo.Attributes = Info.wAttributes;
          ScrollConsoleScreenBuffer(
            hConOut,
            &Rect,
            NULL,
            Pos,
            &CharInfo);
          Pos.Y = Info.dwSize.Y-1;
          SetConsoleCursorPosition(hConOut, Pos);
        }
      }
      else */ 
      if (*s == ESC) state = 2;
      else PushBuffer(*s);
    }
    else if (state == 2) {
      if (*s == ESC);       // \e\e...\e == \e
      else if ( (*s == '[') || (*s == '(') ) {
        FlushBuffer();
        prefix = *s;
        prefix2 = 0;
        state = 3;
      }
      else state = 1;
    }
    else if (state == 3) {
      if ( isdigit(*s)) {
        es_argc = 0;
        es_argv[0] = *s-'0';
        state = 4;
      }
      else if ( *s == ';' ) {
        es_argc = 1;
        es_argv[0] = 0;
        es_argv[es_argc] = 0;
        state = 4;
      }
      else if (*s == '?') {
        prefix2 = *s;
      }
      else {
        es_argc = 0;
        suffix = *s;
        InterpretEscSeq();
        state = 1;
      }
    }
    else if (state == 4) {
      if ( isdigit(*s)) {
        es_argv[es_argc] = 10*es_argv[es_argc]+(*s-'0');
      }
      else if ( *s == ';' ) {
        if (es_argc < MAX_ARG-1) es_argc++;
        es_argv[es_argc] = 0;
      }
      else {
        if (es_argc < MAX_ARG-1) es_argc++;
        suffix = *s;
        InterpretEscSeq();
        state = 1;
      }
    }

    else { // error: unknown automata state (never happens!)
      exit (1);
    }
  }
  FlushBuffer();
  *lpNumberOfBytesWritten = nNumberOfBytesToWrite - i;
  return (i == 0);
}


int main(void)
{
    char test[20] = "test string\0";
    long num_written = 0;
    HWND console = GetConsoleWindow();
    ParseAndPrintString(console, test, 20, &num_written);
    printf("%d", num_written);
    return 0 ;
}
    

