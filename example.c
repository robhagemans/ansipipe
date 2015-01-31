#include <stdio.h>
#include <string.h>
#include "ansipipe.h"

int main() 
{
    if (ansipipe_init() != 0)
        printf("Not connected to ANSI|pipe. Output will be gibberish.\n");
    
    printf("\x1b]2;%s\x07", "ANSI|pipe demo");
    // From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
    printf("\n\n\n\x1b[2AHello,\x1b[1A\x1b[91mWorld!\x1b[2B\x1b[0m Здравствуй,\x1b[1A\x1b[92mмир!\x1b[0m\x1b[2B "); 
    printf("Γεια σου \x1b[1A\x1b[94mκόσμε!\x1b[2B\x1b[0m\n");
    printf("Type something: ");

    char inbuf[1024];
    fgets(inbuf, sizeof(inbuf), stdin);
    if (strlen(inbuf) > 0) inbuf[strlen(inbuf)-1] = 0;
    
    printf("You typed \x1b[45;1m%s\x1b[0m.\n", inbuf);

    return 0;
}
