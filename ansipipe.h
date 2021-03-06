/*
 * ANSI|pipe C/C++ connection header
 * Redirect standard I/O through ANSI|pipe executable
 * to enable UTF-8, ANSI escape sequences and dual mode CLI/GUI executables
 * when compiling command-line applications for Windows
 *
 * based on DualModeI.cpp from dualsubsystem.
 * GNU C version (c) 2015 Rob Hagemans
 * Licensed under the Expat MIT licence.
 * See LICENSE.md or http://opensource.org/licenses/mit-license.php
 */

#ifndef ANSIPIPE_H
#define ANSIPIPE_H


#ifndef _WIN32

// dummy definitions for non-Windows platforms
static int ansipipe_init() { return 0; };
#define ANSIPIPE_LAUNCH(argc, argv)

#else

#include <windows.h>
#include <stdio.h>
#include <io.h>

#ifdef BUILD_DLL
// declarations to enable compilation as DLL
//extern "C" __declspec(dllexport)
#define EXPORT_DLL __declspec(dllexport)
EXPORT_DLL void winsi_init();
EXPORT_DLL void winsi_close();
EXPORT_DLL long winsi_read(char *buffer, long req_count);
EXPORT_DLL void winsi_write(char *buffer);

#else

#define ANSIPIPE_NAME_LEN 256
#define ANSIPIPE_POUT_FMT L"\\\\.\\pipe\\ANSIPIPE_%d_POUT"
#define ANSIPIPE_PIN_FMT L"\\\\.\\pipe\\ANSIPIPE_%d_PIN"
#define ANSIPIPE_PERR_FMT L"\\\\.\\pipe\\ANSIPIPE_%d_PERR"

// initialise ansipipe i/o streams. 0 is success.
static int ansipipe_init()
{
    // construct named pipe names
    wchar_t name_out[ANSIPIPE_NAME_LEN];
    wchar_t name_in[ANSIPIPE_NAME_LEN];
    wchar_t name_err[ANSIPIPE_NAME_LEN];
    _snwprintf(name_out, ANSIPIPE_NAME_LEN, ANSIPIPE_POUT_FMT, GetCurrentProcessId());
    _snwprintf(name_in, ANSIPIPE_NAME_LEN, ANSIPIPE_PIN_FMT, GetCurrentProcessId());
    _snwprintf(name_err, ANSIPIPE_NAME_LEN, ANSIPIPE_PERR_FMT, GetCurrentProcessId());

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
            fprintf(stderr, "ERROR: Failed to initialise ANSI|pipe."); \
    } while(0)

#endif
#endif
#endif
