#ifndef PLATFORM_WIN32_H
#define PLATFORM_WIN32_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gpu_preferences.h"
#include "overlay_ui.h"

typedef struct PlatformInput
{
  float look_x;
  float look_y;
  float move_forward;
  float move_right;
  int escape_pressed;
  int toggle_cycle_pressed;
  int reset_cycle_pressed;
  int increase_cycle_speed_pressed;
  int decrease_cycle_speed_pressed;
  int scrub_backward_held;
  int scrub_forward_held;
  int scrub_fast_held;
  int move_fast_held;
  int crouch_held;
  int jump_pressed;
  int jump_held;
  int move_down_held;
  int toggle_player_mode_pressed;
  int remove_block_pressed;
  int place_block_pressed;
  int selected_block_slot;
} PlatformInput;

typedef struct PlatformApp
{
  HINSTANCE instance;
  HWND window;
  HDC device_context;
  HGLRC gl_context;
  LONG_PTR windowed_style;
  LONG_PTR windowed_ex_style;
  int fullscreen_enabled;
  LARGE_INTEGER timer_frequency;
  LARGE_INTEGER timer_start;
  LONG mouse_dx;
  LONG mouse_dy;
  int width;
  int height;
  int running;
  int resized;
  int escape_requested;
  int previous_toggle_cycle_down;
  int previous_reset_cycle_down;
  int previous_increase_speed_down;
  int previous_decrease_speed_down;
  int previous_jump_down;
  int previous_player_mode_down;
  int previous_alt_down;
  int cursor_hidden;
  int mouse_captured;
  int cursor_mode_enabled;
  int suppress_next_mouse_delta;
  int previous_left_button_down;
  int previous_world_left_button_down;
  int previous_world_right_button_down;
  int suppress_world_click_until_release;
  int gpu_switch_requested;
  int render_quality_change_requested;
  GpuPreferenceMode requested_gpu_preference;
  RendererQualityPreset requested_render_quality_preset;
  OverlayState overlay;
} PlatformApp;

int platform_create(PlatformApp* app, const char* title, int width, int height);
void platform_destroy(PlatformApp* app);
void platform_pump_messages(PlatformApp* app, PlatformInput* input);
void platform_request_close(PlatformApp* app);
float platform_get_time_seconds(const PlatformApp* app);
void platform_swap_buffers(const PlatformApp* app);
void platform_set_window_title(const PlatformApp* app, const char* title);
void platform_get_scene_settings(const PlatformApp* app, SceneSettings* out_settings);
void platform_set_scene_settings(PlatformApp* app, const SceneSettings* settings);
int platform_get_god_mode_enabled(const PlatformApp* app);
void platform_set_god_mode_enabled(PlatformApp* app, int enabled);
void platform_update_overlay_metrics(PlatformApp* app, const OverlayMetrics* metrics);
int platform_consume_gpu_switch_request(PlatformApp* app, GpuPreferenceMode* out_mode);
int platform_consume_render_quality_request(PlatformApp* app, RendererQualityPreset* out_preset);
void platform_set_render_quality_preset(PlatformApp* app, RendererQualityPreset preset);
void platform_refresh_gpu_info(PlatformApp* app);
void platform_show_error_message(const char* title, const char* message);

#endif
