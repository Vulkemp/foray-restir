#include "restir_app.hpp"

int main(int argv, char** args)
{
    foray::osi::OverrideCurrentWorkingDirectory(CWD_OVERRIDE_PATH);
    RestirProject project;
    return project.Run();
}