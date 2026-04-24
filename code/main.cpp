#include "app.h"

#include <iostream>

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const char* WINDOW_TITLE = "PurePool";

int main()
{
    App app(SCR_WIDTH, SCR_HEIGHT, WINDOW_TITLE);
    app.run();
    return 0;
}
