#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
This script demonstrates how to use ANSI|pipe in a console-based Python project.
Under Windows, if the script is run through the ANSI|pipe launcher, UTF-8
and ANSI escape sequences will shown correctly; if the script is run on its
own, ANSI and UTF-8 will not be parsed.
You can run this script on Unix without any changes.
"""

import ansipipe
if not ansipipe.ok:
    print "Not connected to ANSI|pipe. Output will be gibberish.";

print "\x1b]2;ANSI|pipe demo\x07",
# From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
print "\n\n\n\x1b[2AHello, \x1b[1A\x1b[91mWorld!\x1b[2B\x1b[0m ",
print "Здравствуй, \x1b[1A\x1b[92mмир!\x1b[0m\x1b[2B ",
print "Γεια σου \x1b[1A\x1b[94mκόσμε!\x1b[2B\x1b[0m\n",

inp = raw_input("Type something: ")
print "You typed \x1b[45;1m%s\x1b[0m.\n" % inp,
