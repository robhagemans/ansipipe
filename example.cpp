#include <iostream>
#include "ansipipe.hpp"

/*
 * This source demonstrates how to use ANSI|pipe in a console-based C++ project.
 * Under Windows, if the binary is run through the ANSI|pipe launcher, UTF-8 
 * and ANSI escape sequences will shown correctly; if the binary is run on its 
 * own, ANSI and UTF-8 will not be parsed.
 * You can compile and run this source on Unix without any changes.
 */

int main() 
{
    if (!ansipipe_init())
        std::cout << "Not connected to ANSI|pipe. Output will be gibberish.\n";
            
    std::cout << "\x1b]2;ANSI|pipe demo\x07";
    // From helloworldcollection.de. Lucida Sans doesn't support Asian scripts, but this all works:
    std::cout << "\n\n\n\x1b[2AHello,\x1b[1A\x1b[91mWorld!\x1b[2B\x1b[0m Здравствуй,\x1b[1A\x1b[92mмир!\x1b[0m\x1b[2B "; 
    std::cout << "Γεια σου \x1b[1A\x1b[94mκόσμε!\x1b[2B\x1b[0m\n";
    std::cout << "Type something: ";

    std::string input;
    std::getline(std::cin, input);
    std::cout << "You typed \x1b[45;1m" << input << "\x1b[0m.\n";

    return 0;
}
