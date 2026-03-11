#ifndef GL_HEADERS_H
#define GL_HEADERS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
#include <GL/glew.h>
#include <GL/wglew.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
