#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(_WIN32)
#include "platform_win32.h"
#elif defined(__APPLE__)
#include "platform_cocoa.h"
#elif defined(__linux__)
#include "platform_x11.h"
#else
#error Unsupported platform
#endif

#endif
