# -*- coding: utf-8 -*-
import ansipipe

if not ansipipe.ok:
    print "Not connected to ANSI|pipe. Output will be gibberish.";

print "\x1b]2;ANSI|pipe demo\x07",
# From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
print "\n\n\n\x1b[2AHello,\x1b[1A\x1b[91mWorld!\x1b[2B\x1b[0m Здравствуй,\x1b[1A\x1b[92mмир!\x1b[0m\x1b[2B ",
print "Γεια σου \x1b[1A\x1b[94mκόσμε!\x1b[2B\x1b[0m\n",

inp = raw_input("Type something: ")
print "You typed \x1b[45;1m%s\x1b[0m.\n" % inp,



