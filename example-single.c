#include <stdio.h>
#include <string.h>
#include "ansipipe.h"

/*
 * This source demonstrates how to use ANSI|pipe in a console-based C project
 * to produce a *single* binary. Compile with 
 * gcc example-single.c launcher.c -D ANSIPIPE_SINGLE
 *
 * You can compile and run this source on Unix without any changes.
 */

int main(int argc, char *argv[]) 
{
    ANSIPIPE_LAUNCH(argc, argv);
    
    printf("\x1b]2;%s\x07", "ANSI|pipe demo");
    // From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
    printf("\n\n\n\x1b[2AHello, \x1b[1A\x1b[91mWorld!\x1b[2B\x1b[0m ");
    printf("Здравствуй, \x1b[1A\x1b[92mмир!\x1b[0m\x1b[2B "); 
    printf("Γεια σου \x1b[1A\x1b[94mκόσμε!\x1b[2B\x1b[0m\n");
    printf("Type something: ");

    char inbuf[1024];
    fgets(inbuf, sizeof(inbuf), stdin);
    if (strlen(inbuf) > 0) inbuf[strlen(inbuf)-1] = 0;
    
    printf("You typed \x1b[45;1m%s\x1b[0m.\n", inbuf);

    return 0;
}

