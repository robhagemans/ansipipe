# -*- coding: utf-8 -*-
import ansipipe

print "\x1b]2;ANSI|term demo\x07",
# From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
print "\n\n\n\x1b[2AHello,\x1b[1A\x1b[91;4mWorld!\x1b[2B\x1b[0m Здравствуй,\x1b[1A\x1b[92;4mмир!\x1b[0m\x1b[2B ",
print "Γεια σου \x1b[1A\x1b[94;4mκόσμε!\x1b[2B\x1b[0m\n",

inp = raw_input("Type something: ")
print "You typed \x1b[45;1m%s\x1b[0m.\n" % inp,



