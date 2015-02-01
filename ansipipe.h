/*
 * ANSI|pipe C/C++ connection header
 * Redirect standard I/O through ANSI|pipe executable
 * to enable UTF-8, ANSI escape sequences and dual mode CLI/GUI executables
 * when compiling command-line applications for Windows
 * 
 * based on DualModeI.cpp from dualsybsystem.
 * GNU C version (c) 2015 Rob Hagemans
 * This header is released under the terms of the MIT license.
*/

#ifndef ANSIPIPE_H
#define ANSIPIPE_H

#ifndef _WIN32

// dummy definitions for non-Windows platforms 
int ansipipe_init() { return 0; };
int ansipipe_launcher(int argc, char *argv[], long *exit_code) { return 0; };
#define ANSIPIPE_LAUNCH(argc, argv)

#else

#include <windows.h>
#include <stdio.h>
#include <io.h>

#define NAME_LEN 256

// initialise ansipipe i/o streams. 0 is success.
int ansipipe_init()
{
    // construct named pipe names
    wchar_t name_out[NAME_LEN];
    wchar_t name_in[NAME_LEN];
    wchar_t name_err[NAME_LEN];
    swprintf(name_out, L"\\\\.\\pipe\\%dcout", GetCurrentProcessId());
    swprintf(name_in, L"\\\\.\\pipe\\%dcin", GetCurrentProcessId());
    swprintf(name_err, L"\\\\.\\pipe\\%dcerr", GetCurrentProcessId());

    // keep a copy of the existing stdio streams in case we fail
    int old_out = _dup(_fileno(stdout));
    int old_in = _dup(_fileno(stdin));
    int old_err = _dup(_fileno(stderr));
    
    // try to attach named pipes to stdin/stdout/stderr
    int rc = 0;
    rc = (int) _wfreopen(name_out, L"a", stdout);
    rc = rc && (int) _wfreopen(name_in, L"r", stdin);
    rc = rc && (int) _wfreopen(name_err, L"a", stderr);

    if (rc) {
        // unix app would expect line buffering _IOLBF but WIN32 doesn't have it
        setvbuf(stdout, NULL, _IONBF, 0);
        // stderr *should* be unbuffered, but we can't count on it
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    else {
        // failed; restore original stdio
        _dup2(old_out, _fileno(stdout));        
        _dup2(old_in, _fileno(stdin));        
        _dup2(old_err, _fileno(stderr));        
#ifdef __cplusplus
        // iostreams hang on MinGW C++ if we don't printf at least 2 chars here
        printf("\b\b");
#endif
    }
    return !rc;
}

/* definitions for single-binary mode only */

// defined in launcher.c
int ansipipe_launcher(int argc, char *argv[], long *exit_code);

#define ANSIPIPE_LAUNCH(argc, argv) \
    do { \
        long exit_code = 0;\
        if (ansipipe_launcher((argc), (argv), &exit_code)) \
            return exit_code; \
        else \
            --argc; \
        if (ansipipe_init() != 0) \
            printf("ERROR: Failed to initialise ANSI|pipe."); \
    } while(0)


#endif
#endif

