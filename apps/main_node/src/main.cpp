#include "MainApp.hpp"

int main(void)
{
    if (MainApp::getInstance().init() == 0) {
        MainApp::getInstance().run();
    }
    return 0;
}
