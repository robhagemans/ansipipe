/*
 * ANSI|pipe C connection module
 * Redirect standard I/O through ANSI|pipe executable
 * to enable UTF-8, ANSI escape sequences and dual mode CLI/GUI executables
 * when packaging Python applications to a Windows executable.
 * 
 * based on DualModeI.cpp from dualsybsystem.
 * GNU C version (c) 2015 Rob Hagemans
 * This module is released under the terms of the MIT license.
*/

#include <windows.h>
#include <stdio.h>
#include "ansipipe-connect.h"

bool ansipipe_init()
{
      // construct named pipe names
      char name_out[256];
      char name_in[256];
      char name_err[256];
      sprintf(name_out, "\\\\.\\pipe\\%dcout", GetCurrentProcessId());
      sprintf(name_in, "\\\\.\\pipe\\%dcin", GetCurrentProcessId());
      sprintf(name_err, "\\\\.\\pipe\\%dcerr", GetCurrentProcessId());

      // attach named pipes to stdin/stdout/stderr
      bool rc = false;
      rc = freopen(name_out, "a", stdout) != NULL;
      rc = rc && freopen(name_in, "r", stdin) != NULL;
      rc = rc && freopen(name_err, "a", stderr) != NULL;
      return rc;
}
