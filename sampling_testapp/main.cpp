#include "sampling_testapp.hpp"
#include <foray_basics.hpp>
#include <foray_logger.hpp>
#include <osi/foray_env.hpp>
#include <vector>

namespace sampling_testapp {


    int example(std::vector<std::string>& args)
    {
        foray::osi::OverrideCurrentWorkingDirectory(CWD_OVERRIDE);
        SamplingTestApp app;
        return app.Run();
    }
}  // namespace sampling_testapp

int main(int argv, char** args)
{
    std::vector<std::string> argvec(argv);
    for(int i = 0; i < argv; i++)
    {
        argvec[i] = args[i];
    }
    return sampling_testapp::example(argvec);
}
