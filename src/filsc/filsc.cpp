/// <summary>
/// FILS command line compiler.
/// </summary>

#include "builder.h"
#include <iostream>
#include <filesystem>

//TODO: filesystem namespace is supossed to be already in 'std' namespace.
//Change as appropiate of put an '#ifdef' to handle different compilers / standard library versions.
namespace fs = std::experimental::filesystem;

using namespace std;

string getRuntimePath(const string& exePath);

/// <summary>
/// Compiler entry point
/// </summary>
/// <returns></returns>
int main(int argc, const char** argv)
{
    string runtimePath = getRuntimePath(argv[0]);

    if (runtimePath.empty())
    {
        cerr << "Cannot find runtime. STOP.\n";
        return -1;
    }
    //else
    //{
    //    cerr << "Runtime found at: " << runtimePath << "\n";
    //    return 0;
    //}

    string modulePath = ".";

    if (argc > 1)
        modulePath = argv[1];

    BuilderConfig   cfg;
    cfg.BasePath = runtimePath;
    auto result = buildModule(modulePath, cfg);

    if (result.ok())
    {
        cout << "Ok\n";
        return 0;
    }
    else
    {
        cout << "Errors detected:\n";

        for (auto& err : result.errors)
            cout << err.what() << "\n";

        return (int)result.errors.size();
    }
}

/// <summary>
/// Gets the path where 'FIL-S' runtime files are located.
/// </summary>
/// <param name="exePath"></param>
/// <returns></returns>
string getRuntimePath(const string& exePath)
{
    fs::path    path = exePath;

    while (path.has_parent_path())
    {
        path = path.parent_path();

        auto        rtPath = path / "runtime";
        error_code	ec;

        if (fs::is_directory(fs::status(rtPath, ec)))
            return rtPath.u8string();
    }

    return "";
}
