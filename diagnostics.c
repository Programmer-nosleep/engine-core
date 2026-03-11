#include "diagnostics.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int diagnostics_build_log_path(char* out_path, size_t out_path_size)
{
  DWORD path_length = GetModuleFileNameA(NULL, out_path, (DWORD)out_path_size);
  char* last_separator = NULL;

  if (path_length == 0U || path_length >= (DWORD)out_path_size)
  {
    return 0;
  }

  last_separator = strrchr(out_path, '\\');
  if (last_separator == NULL)
  {
    last_separator = strrchr(out_path, '/');
  }

  if (last_separator == NULL)
  {
    return 0;
  }

  last_separator[1] = '\0';
  if (strlen(out_path) + strlen("opengl_sky.log") + 1U > out_path_size)
  {
    return 0;
  }

  (void)snprintf(out_path + strlen(out_path), out_path_size - strlen(out_path), "%s", "opengl_sky.log");
  return 1;
}

void diagnostics_log(const char* message)
{
  char log_path[MAX_PATH] = { 0 };
  FILE* file = NULL;
  SYSTEMTIME now = { 0 };

  if (message == NULL || !diagnostics_build_log_path(log_path, sizeof(log_path)))
  {
    return;
  }

  GetLocalTime(&now);

  #if defined(_MSC_VER)
  if (fopen_s(&file, log_path, "a") != 0)
  {
    file = NULL;
  }
  #else
  file = fopen(log_path, "a");
  #endif

  if (file == NULL)
  {
    return;
  }

  (void)fprintf(
    file,
    "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\n",
    (unsigned)now.wYear,
    (unsigned)now.wMonth,
    (unsigned)now.wDay,
    (unsigned)now.wHour,
    (unsigned)now.wMinute,
    (unsigned)now.wSecond,
    (unsigned)now.wMilliseconds,
    message
  );
  fclose(file);
}

void diagnostics_logf(const char* format, ...)
{
  char buffer[1024] = { 0 };
  va_list args;

  if (format == NULL)
  {
    return;
  }

  va_start(args, format);
  (void)vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  diagnostics_log(buffer);
}
