
/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkMutex.h"
#include "SkOnce.h"
#include "gl/GrGLInterface.h"
#include "gl/GrGLAssembleInterface.h"
#include "gl/command_buffer/GLTestContext_command_buffer.h"
#include "../ports/SkOSEnvironment.h"
#include "../ports/SkOSLibrary.h"

#if defined SK_BUILD_FOR_MAC

// EGL doesn't exist on the mac, so expose what we need to get the command buffer's EGL running.
typedef void *EGLDisplay;
typedef unsigned int EGLBoolean;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef int32_t EGLint;
typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);
#define EGL_FALSE 0
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3025
#define EGL_SAMPLES 0x3031
#define EGL_SAMPLE_BUFFERS 0x3032
#define EGL_NONE 0x3038
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056

#else

#include <EGL/egl.h>

#endif

typedef EGLDisplay (*GetDisplayProc)(EGLNativeDisplayType display_id);
typedef EGLBoolean (*InitializeProc)(EGLDisplay dpy, EGLint *major, EGLint *minor);
typedef EGLBoolean (*TerminateProc)(EGLDisplay dpy);
typedef EGLBoolean (*ChooseConfigProc)(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size, EGLint* num_config);
typedef EGLBoolean (*GetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value);
typedef EGLSurface (*CreateWindowSurfaceProc)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list);
typedef EGLSurface (*CreatePbufferSurfaceProc)(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list);
typedef EGLBoolean (*DestroySurfaceProc)(EGLDisplay dpy, EGLSurface surface);
typedef EGLContext (*CreateContextProc)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list);
typedef EGLBoolean (*DestroyContextProc)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean (*MakeCurrentProc)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
typedef EGLBoolean (*SwapBuffersProc)(EGLDisplay dpy, EGLSurface surface);
typedef __eglMustCastToProperFunctionPointerType (*GetProcAddressProc)(const char* procname);

static GetDisplayProc gfGetDisplay = nullptr;
static InitializeProc gfInitialize = nullptr;
static TerminateProc gfTerminate = nullptr;
static ChooseConfigProc gfChooseConfig = nullptr;
static GetConfigAttrib gfGetConfigAttrib = nullptr;
static CreateWindowSurfaceProc gfCreateWindowSurface = nullptr;
static CreatePbufferSurfaceProc gfCreatePbufferSurface = nullptr;
static DestroySurfaceProc gfDestroySurface = nullptr;
static CreateContextProc gfCreateContext = nullptr;
static DestroyContextProc gfDestroyContext = nullptr;
static MakeCurrentProc gfMakeCurrent = nullptr;
static SwapBuffersProc gfSwapBuffers = nullptr;
static GetProcAddressProc gfGetProcAddress = nullptr;

static void* gLibrary = nullptr;
static bool gfFunctionsLoadedSuccessfully = false;

namespace {
static void load_command_buffer_functions() {
    if (!gLibrary) {
#if defined _WIN32
        gLibrary = DynamicLoadLibrary("command_buffer_gles2.dll");
#elif defined SK_BUILD_FOR_MAC
        gLibrary = DynamicLoadLibrary("libcommand_buffer_gles2.dylib");
#else
        gLibrary = DynamicLoadLibrary("libcommand_buffer_gles2.so");
#endif // defined _WIN32
        if (gLibrary) {
            gfGetDisplay = (GetDisplayProc)GetProcedureAddress(gLibrary, "eglGetDisplay");
            gfInitialize = (InitializeProc)GetProcedureAddress(gLibrary, "eglInitialize");
            gfTerminate = (TerminateProc)GetProcedureAddress(gLibrary, "eglTerminate");
            gfChooseConfig = (ChooseConfigProc)GetProcedureAddress(gLibrary, "eglChooseConfig");
            gfGetConfigAttrib = (GetConfigAttrib)GetProcedureAddress(gLibrary, "eglGetConfigAttrib");
            gfCreateWindowSurface = (CreateWindowSurfaceProc)GetProcedureAddress(gLibrary, "eglCreateWindowSurface");
            gfCreatePbufferSurface = (CreatePbufferSurfaceProc)GetProcedureAddress(gLibrary, "eglCreatePbufferSurface");
            gfDestroySurface = (DestroySurfaceProc)GetProcedureAddress(gLibrary, "eglDestroySurface");
            gfCreateContext = (CreateContextProc)GetProcedureAddress(gLibrary, "eglCreateContext");
            gfDestroyContext = (DestroyContextProc)GetProcedureAddress(gLibrary, "eglDestroyContext");
            gfMakeCurrent = (MakeCurrentProc)GetProcedureAddress(gLibrary, "eglMakeCurrent");
            gfSwapBuffers = (SwapBuffersProc)GetProcedureAddress(gLibrary, "eglSwapBuffers");
            gfGetProcAddress = (GetProcAddressProc)GetProcedureAddress(gLibrary, "eglGetProcAddress");

            gfFunctionsLoadedSuccessfully = gfGetDisplay && gfInitialize && gfTerminate &&
                                            gfChooseConfig && gfCreateWindowSurface &&
                                            gfCreatePbufferSurface && gfDestroySurface &&
                                            gfCreateContext && gfDestroyContext && gfMakeCurrent &&
                                            gfSwapBuffers && gfGetProcAddress;

        }
    }
}

static GrGLFuncPtr command_buffer_get_gl_proc(void* ctx, const char name[]) {
    if (!gfFunctionsLoadedSuccessfully) {
        return nullptr;
    }
    return gfGetProcAddress(name);
}

SK_DECLARE_STATIC_ONCE(loadCommandBufferOnce);
static void load_command_buffer_once() {
    SkOnce(&loadCommandBufferOnce, load_command_buffer_functions);
}

static const GrGLInterface* create_command_buffer_interface() {
    load_command_buffer_once();
    if (!gfFunctionsLoadedSuccessfully) {
        return nullptr;
    }
    return GrGLAssembleGLESInterface(gLibrary, command_buffer_get_gl_proc);
}

// We use a poor man's garbage collector of EGLDisplays. They are only
// terminated when there are no more EGLDisplays in use. See crbug.com/603223
SK_DECLARE_STATIC_MUTEX(gDisplayMutex);
static int gActiveDisplayCnt;
SkTArray<EGLDisplay> gRetiredDisplays;

static EGLDisplay get_and_init_display() {
    SkAutoMutexAcquire am(gDisplayMutex);
    EGLDisplay dsp = gfGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dsp == EGL_NO_DISPLAY) {
        return EGL_NO_DISPLAY;
    }
    EGLint major, minor;
    if (!gfInitialize(dsp, &major, &minor)) {
        gRetiredDisplays.push_back(dsp);
        return EGL_NO_DISPLAY;
    }
    ++gActiveDisplayCnt;
    return dsp;
}

static void retire_display(EGLDisplay dsp) {
    if (dsp == EGL_NO_DISPLAY) {
        return;
    }
    SkAutoMutexAcquire am(gDisplayMutex);
    gRetiredDisplays.push_back(dsp);
    --gActiveDisplayCnt;
    if (!gActiveDisplayCnt) {
        for (EGLDisplay d : gRetiredDisplays) {
            gfTerminate(d);
        }
        gRetiredDisplays.reset();
    }
}
}  // anonymous namespace

namespace sk_gpu_test {

CommandBufferGLTestContext::CommandBufferGLTestContext()
    : fContext(EGL_NO_CONTEXT), fDisplay(EGL_NO_DISPLAY), fSurface(EGL_NO_SURFACE) {

    static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    static const EGLint surfaceAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };

    initializeGLContext(nullptr, configAttribs, surfaceAttribs);
}

CommandBufferGLTestContext::CommandBufferGLTestContext(void *nativeWindow, int msaaSampleCount) {
    static const EGLint surfaceAttribs[] = {EGL_NONE};

    EGLint configAttribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, 1,
        EGL_SAMPLES, msaaSampleCount,
        EGL_NONE
    };
    if (msaaSampleCount == 0) {
        configAttribs[12] = EGL_NONE;
    }

    initializeGLContext(nativeWindow, configAttribs, surfaceAttribs);
}

void CommandBufferGLTestContext::initializeGLContext(void *nativeWindow, const int *configAttribs,
                                                 const int *surfaceAttribs) {
    load_command_buffer_once();
    if (!gfFunctionsLoadedSuccessfully) {
        SkDebugf("Command Buffer: Could not load EGL functions.\n");
        return;
    }

    // Make sure CHROMIUM_path_rendering is enabled for NVPR support.
    sk_setenv("CHROME_COMMAND_BUFFER_GLES2_ARGS", "--enable-gl-path-rendering");
    fDisplay = get_and_init_display();
    if (EGL_NO_DISPLAY == fDisplay) {
        SkDebugf("Command Buffer: Could not create EGL display.\n");
        return;
    }

    EGLint numConfigs;
    if (!gfChooseConfig(fDisplay, configAttribs, static_cast<EGLConfig *>(&fConfig), 1,
                        &numConfigs) || numConfigs != 1) {
        SkDebugf("Command Buffer: Could not choose EGL config.\n");
        this->destroyGLContext();
        return;
    }

    if (nativeWindow) {
        fSurface = gfCreateWindowSurface(fDisplay,
                                         static_cast<EGLConfig>(fConfig),
                                         (EGLNativeWindowType) nativeWindow,
                                         surfaceAttribs);
    } else {
        fSurface = gfCreatePbufferSurface(fDisplay,
                                          static_cast<EGLConfig>(fConfig),
                                          surfaceAttribs);
    }
    if (EGL_NO_SURFACE == fSurface) {
        SkDebugf("Command Buffer: Could not create EGL surface.\n");
        this->destroyGLContext();
        return;
    }

    static const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    fContext = gfCreateContext(fDisplay, static_cast<EGLConfig>(fConfig), nullptr, contextAttribs);
    if (EGL_NO_CONTEXT == fContext) {
        SkDebugf("Command Buffer: Could not create EGL context.\n");
        this->destroyGLContext();
        return;
    }

    if (!gfMakeCurrent(fDisplay, fSurface, fSurface, fContext)) {
        SkDebugf("Command Buffer: Could not make EGL context current.\n");
        this->destroyGLContext();
        return;
    }

    SkAutoTUnref<const GrGLInterface> gl(create_command_buffer_interface());
    if (nullptr == gl.get()) {
        SkDebugf("Command Buffer: Could not create CommandBuffer GL interface.\n");
        this->destroyGLContext();
        return;
    }
    if (!gl->validate()) {
        SkDebugf("Command Buffer: Could not validate CommandBuffer GL interface.\n");
        this->destroyGLContext();
        return;
    }

    this->init(gl.release());
}

CommandBufferGLTestContext::~CommandBufferGLTestContext() {
    this->teardown();
    this->destroyGLContext();
}

void CommandBufferGLTestContext::destroyGLContext() {
    if (!gfFunctionsLoadedSuccessfully) {
        return;
    }
    if (fDisplay) {
        gfMakeCurrent(fDisplay, 0, 0, 0);

        if (fContext) {
            gfDestroyContext(fDisplay, fContext);
            fContext = EGL_NO_CONTEXT;
        }

        if (fSurface) {
            gfDestroySurface(fDisplay, fSurface);
            fSurface = EGL_NO_SURFACE;
        }

        retire_display(fDisplay);
        fDisplay = EGL_NO_DISPLAY;
    }
}

void CommandBufferGLTestContext::onPlatformMakeCurrent() const {
    if (!gfFunctionsLoadedSuccessfully) {
        return;
    }
    if (!gfMakeCurrent(fDisplay, fSurface, fSurface, fContext)) {
        SkDebugf("Command Buffer: Could not make EGL context current.\n");
    }
}

void CommandBufferGLTestContext::onPlatformSwapBuffers() const {
    if (!gfFunctionsLoadedSuccessfully) {
        return;
    }
    if (!gfSwapBuffers(fDisplay, fSurface)) {
        SkDebugf("Command Buffer: Could not complete gfSwapBuffers.\n");
    }
}

GrGLFuncPtr CommandBufferGLTestContext::onPlatformGetProcAddress(const char *name) const {
    if (!gfFunctionsLoadedSuccessfully) {
        return nullptr;
    }
    return gfGetProcAddress(name);
}

void CommandBufferGLTestContext::presentCommandBuffer() {
    if (this->gl()) {
        this->gl()->fFunctions.fFlush();
    }

    this->onPlatformSwapBuffers();
}

bool CommandBufferGLTestContext::makeCurrent() {
    return gfMakeCurrent(fDisplay, fSurface, fSurface, fContext) != EGL_FALSE;
}

int CommandBufferGLTestContext::getStencilBits() {
    EGLint result = 0;
    gfGetConfigAttrib(fDisplay, static_cast<EGLConfig>(fConfig), EGL_STENCIL_SIZE, &result);
    return result;
}

int CommandBufferGLTestContext::getSampleCount() {
    EGLint result = 0;
    gfGetConfigAttrib(fDisplay, static_cast<EGLConfig>(fConfig), EGL_SAMPLES, &result);
    return result;
}

}  // namespace sk_gpu_test
