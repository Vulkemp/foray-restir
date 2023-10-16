#include "foray_complexrtapp.hpp"
#include <foray_basics.hpp>
#include <foray_logger.hpp>
#include <osi/foray_env.hpp>
#include <vector>

namespace complex_raytracer {


    int example(std::vector<std::string>& args)
    {
        foray::osi::OverrideCurrentWorkingDirectory(CWD_OVERRIDE);
        ComplexRaytracerApp app;
        return app.Run();
    }
}  // namespace complex_raytracer

int main(int argv, char** args)
{
    std::vector<std::string> argvec(argv);
    for(int i = 0; i < argv; i++)
    {
        argvec[i] = args[i];
    }
    return complex_raytracer::example(argvec);
}
