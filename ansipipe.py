""" 
ANSI|pipe Python connection module
Redirect standard I/O through ANSI|pipe executable
to enable UTF-8, ANSI escape sequences and dual mode CLI/GUI executables
when packaging Python applications to a Windows executable.

based on DualModeI.cpp from dualsybsystem.
Python version (c) 2015 Rob Hagemans
This module is released under the terms of the MIT license.
"""

import os
import sys

pid = os.getpid()

# construct named pipe names
name_out = '\\\\.\\pipe\\%dcout' % pid
name_in = '\\\\.\\pipe\\%dcin' % pid
name_err = '\\\\.\\pipe\\%dcerr' % pid

# attach named pipes to stdin/stdout/stderr
try:
    sys.stdout = open(name_out, 'wb')
    sys.stdin = open(name_in, 'rb')
    sys.stderr = open(name_err, 'wb')
except EnvironmentError:
    sys.stdout = sys.__stdout__
    sys.stdin = sys.__stdin__
    sys.stderr = sys.__stderr__
    


