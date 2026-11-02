#include "stdafx.h"

#include "xrEngine/x_ray.h"
#include "xrGame/xrGame.h"
#include "Include/xrRender/xrRender.h"

#if !defined(XR_PLATFORM_WINDOWS)
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#endif

#if defined(XR_PLATFORM_ANDROID) || defined(XR_PLATFORM_IOS)
#include <SDL_main.h> // renames main() to SDL_main(), which the platform glue calls
#endif

#if defined(XR_PLATFORM_IOS)
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <mach-o/dyld.h>
#endif

// Always request high performance GPU
extern "C"
{
// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
XR_EXPORT u32 NvOptimusEnablement = 0x00000001; // NVIDIA Optimus

// https://gpuopen.com/amdpowerxpressrequesthighperformance/
XR_EXPORT u32 AmdPowerXpressRequestHighPerformance = 0x00000001; // PowerXpress or Hybrid Graphics
}

std::array<RendererModule*, 2> s_render_modules =
{
#ifdef XR_PLATFORM_WINDOWS
    xray::render::render_r4::GetRendererModule(),
#endif
    xray::render::render_gl::GetRendererModule(),
};

struct tracy_raii
{
    ~tracy_raii()
    {
#ifdef TRACY_ENABLE
        tracy::GetProfiler().RequestShutdown();
        while (!tracy::GetProfiler().HasShutdownFinished())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#endif
    }
};

#if defined(XR_PLATFORM_IOS)
// iOS apps get no command line and no user-visible install folder. The player copies their
// gamedata.db* archives into the app's Documents folder (Files > On My iPhone/iPad), and the
// GL shaders and fsgame.ltx that ship inside the app bundle are installed next to them.
static std::string ios_prepare_documents()
{
    namespace fs = std::filesystem;

    const char* home = getenv("HOME");
    const fs::path documents = fs::path(home ? home : ".") / "Documents";
    char* base = SDL_GetBasePath();
    const fs::path bundle = base ? base : ".";
    SDL_free(base);

    // Reinstall the bundled resources whenever the app binary changes, i.e. after an update
    std::string stamp = "unknown";
    char exe[PATH_MAX];
    uint32_t exeSize = sizeof(exe);
    struct stat info{};
    if (_NSGetExecutablePath(exe, &exeSize) == 0 && stat(exe, &info) == 0)
        stamp = std::to_string(info.st_mtimespec.tv_sec) + "-" + std::to_string(info.st_size);

    const fs::path stampFile = documents / ".engine-resources";
    std::string installed;
    std::ifstream(stampFile) >> installed;
    std::error_code ec;
    if (installed != stamp)
    {
        fs::copy(bundle / "gamedata", documents / "gamedata",
            fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec)
            SDL_Log("failed to install engine resources: %s", ec.message().c_str());
        else
            std::ofstream(stampFile) << stamp;
    }
    if (!fs::exists(documents / "fsgame.ltx", ec)) // keep user edits
        fs::copy_file(bundle / "fsgame.ltx", documents / "fsgame.ltx", ec);

    std::string commandLine = "-shoc -fsltx " + (documents / "fsgame.ltx").string();
    // Extra engine arguments for testing, e.g. -start server(all/single/alife/new) client(localhost)
    std::string extra;
    if (std::getline(std::ifstream(documents / "commandline.txt"), extra) && !extra.empty())
        commandLine += " " + extra;
    return commandLine;
}
#endif

int entry_point(pcstr commandLine)
{
    tracy_raii raii;
    auto* game = strstr(commandLine, "-nogame") ? nullptr : &xrGame;

    CApplication app{ commandLine, game, s_render_modules };

    return app.Run();
}

#if defined(XR_PLATFORM_WINDOWS)
int StackoverflowFilter(const int exceptionCode)
{
    if (exceptionCode == EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

int APIENTRY WinMain(HINSTANCE inst, HINSTANCE prevInst, char* commandLine, int cmdShow)
{
    int result = 0;
    // BugTrap can't handle stack overflow exception, so handle it here
    __try
    {
        result = entry_point(commandLine);
    }
    __except (StackoverflowFilter(GetExceptionCode()))
    {
        _resetstkoflw();
        FATAL("stack overflow");
    }

    return result;
}
#else
int main(int argc, char *argv[])
{
    int result = EXIT_FAILURE;

    try
    {
#if defined(XR_PLATFORM_IOS)
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2"); // keep it out of the way of edge swipes
        const std::string iosCommandLine = ios_prepare_documents();
        result = entry_point(iosCommandLine.c_str());
#else
        char* commandLine = nullptr;
        int i;
        if(argc > 1)
        {
            size_t sum = 1;
            for(i = 1; i < argc; ++i)
                sum += strlen(argv[i]) + 1;

            commandLine = (char*)xr_malloc(sum);
            ZeroMemory(commandLine, sum);

            for(i = 1; i < argc; ++i)
            {
                strcat(commandLine, argv[i]);
                strcat(commandLine, " ");
            }

            result = entry_point(commandLine);

            xr_free(commandLine);
        }
        else
            result = entry_point("");
#endif
    }
    catch (const std::overflow_error& e)
    {
        _resetstkoflw();
        FATAL_F("stack overflow: %s", e.what());
    }
    catch (const std::runtime_error& e)
    {
        FATAL_F("runtime error: %s", e.what());
    }
    catch (const std::exception& e)
    {
        FATAL_F("exception: %s", e.what());
    }
    catch (...)
    {
    // this executes if f() throws std::string or int or any other unrelated type
    }

    return result;
}
#endif
