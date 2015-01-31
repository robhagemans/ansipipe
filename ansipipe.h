/*
 * ANSI|pipe C connection header
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

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <io.h>

#define NAME_LEN 256

// initialise ansipipe i/o streams. 0 is success.
int ansipipe_init()
{
    // construct named pipe names
    char name_out[NAME_LEN];
    char name_in[NAME_LEN];
    char name_err[NAME_LEN];
    sprintf(name_out, "\\\\.\\pipe\\%dcout", GetCurrentProcessId());
    sprintf(name_in, "\\\\.\\pipe\\%dcin", GetCurrentProcessId());
    sprintf(name_err, "\\\\.\\pipe\\%dcerr", GetCurrentProcessId());

    // keep a copy of the existing stdio streams in case we fail
    int old_out = _dup(_fileno(stdout));
    int old_in = _dup(_fileno(stdin));
    int old_err = _dup(_fileno(stderr));
    
    // try to attach named pipes to stdin/stdout/stderr
    int rc = 0;
    rc = (int) freopen(name_out, "a", stdout);
    rc = rc && (int) freopen(name_in, "r", stdin);
    rc = rc && (int) freopen(name_err, "a", stderr);

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
    }
    return !rc;
}

/* definitions for single-binary mode */

// defined in launcher.c, but only needed for single-binary compile
int ansipipe_launcher(int argc, char *argv[], long *exit_code);

#define ANSIPIPE_LAUNCH(argc, argv) \
    do { \
        long exit_code = 0;\
        if (ansipipe_launcher((argc), (argv), &exit_code)) \
            return exit_code; \
        else \
            --argc; \
    } while(0)

#else

int ansipipe_init() 
{
    return 0;
};

int ansipipe_launcher(int argc, char *argv[], long *exit_code)
{
    return 0;
};

#define ANSIPIPE_LAUNCH(argc, argv)

#endif
#endif

