#pragma once

#include "Layers/xrRender/HWCaps.h"
#include "xrCore/ModuleLookup.hpp"

#ifdef XR_PLATFORM_WEB
#include <emscripten/html5.h>
#endif

namespace xray::render::RENDER_NAMESPACE
{
class CHW
    : public pureAppActivate,
      public pureAppDeactivate
{
public:
    CHW();
    ~CHW();

    void CreateDevice(SDL_Window* sdlWnd);
    void DestroyDevice();

    void Reset();

    void SetPrimaryAttributes(u32& windowFlags);

    IRender::RenderContext GetCurrentContext() const;
    int  MakeContextCurrent(IRender::RenderContext context) const;

    static std::pair<u32, u32> GetSurfaceSize();
    DeviceState GetDeviceState() const;

public:
    void BeginScene();
    void EndScene();
    void Present();

public:
    void OnAppActivate() override;
    void OnAppDeactivate() override;

private:
    void UpdateViews();
    bool ThisInstanceIsGlobal() const;

public:
    void BeginPixEvent(pcstr name) const;
    void EndPixEvent() const;

public:
    static constexpr auto IMM_CTX_ID = 0;

    CHWCaps Caps;

    u32 BackBufferCount{};
    u32 CurrentBackBuffer{};

    GLuint pFB{};
    GLuint DisplayFB{}; // what Present() blits to: 0 everywhere but iOS, where SDL owns an FBO

    SDL_Window* m_window{};

    SDL_GLContext m_context{};

#ifdef XR_PLATFORM_WEB
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE m_webgl{};
#endif
#ifdef XR_GLES
    bool BaseVertexDrawSupported{};
    bool S3TCSupported{}; // DXT1/3/5 sampling; Mali GPUs and some iOS devices lack it
#endif

    bool ErrorChecks{}; // -gl_debug: CHK_GL logs GL errors in release builds too

    pcstr AdapterName;
    pcstr OpenGLVersionString;
    pcstr ShadingVersion;
    bool ComputeShadersSupported;
#ifdef XR_GLES
    u32 TextureUploadUnit{};
#endif
};

struct texture_upload_unit
{
#ifdef XR_GLES
    const GLenum target;
    explicit texture_upload_unit(GLenum target);
    ~texture_upload_unit();
#else
    explicit texture_upload_unit(GLenum) {}
#endif
};

extern ECORE_API CHW HW;
} // namespace xray::render::RENDER_NAMESPACE

#ifndef DEBUG
// Release builds normally skip GL error checks. With -gl_debug they log the failing call,
// which is how errors get pinned down on drivers without KHR_debug (e.g. the Android emulator).
#undef CHK_GL
#define CHK_GL(expr)\
    do\
    {\
        expr;\
        if (::xray::render::RENDER_NAMESPACE::HW.ErrorChecks)\
        {\
            const GLenum err_ = glGetError();\
            if (err_ != GL_NO_ERROR)\
                Msg("! OpenGL error 0x%x: %s [%s:%d]", err_, #expr, __FILE__, __LINE__);\
        }\
    } while (false)
#endif
