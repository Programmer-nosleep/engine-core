#include "app.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
  (void)previous_instance;
  (void)command_line;
  return app_run(instance, show_command);
}
