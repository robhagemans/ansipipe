#include <iostream>
#include "ansipipe.hpp"

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
