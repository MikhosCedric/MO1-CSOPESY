#include "MainMenu.h"
#include <cstdlib>
#include <ctime>

int main() {
    // Seed the randomness once, using the current time, so the generated
    // process instructions differ on each run.
    srand(static_cast<unsigned int>(time(nullptr)));

    MainMenu consoleUI;
    consoleUI.run();

    return 0;
}