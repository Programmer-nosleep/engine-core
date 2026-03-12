#include "platform_x11.h"

#include "diagnostics.h"
#include "gl_headers.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#if defined(__linux__)
#include <GL/glx.h>
#elif defined(__INTELLISENSE__) || defined(__clangd__)
#include "platform_glx_editor_stub.h"
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

static double platform_get_monotonic_seconds(void);
static int platform_window_has_focus(const PlatformApp* app);
static int platform_is_key_down(const PlatformApp* app, KeySym symbol);
static void platform_sync_overlay_state(PlatformApp* app);
static void platform_center_cursor(PlatformApp* app);
static void platform_set_mouse_capture(PlatformApp* app, int enabled);
static void platform_toggle_fullscreen(PlatformApp* app);
static Cursor platform_create_blank_cursor(Display* display, Window window);

static double platform_get_monotonic_seconds(void)
{
  struct timespec timestamp = { 0 };

  if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0)
  {
    return 0.0;
  }

  return (double)timestamp.tv_sec + (double)timestamp.tv_nsec / 1000000000.0;
}

static int platform_window_has_focus(const PlatformApp* app)
{
  return app != NULL && app->display != NULL && app->window != 0U && app->has_focus != 0;
}

static int platform_is_key_down(const PlatformApp* app, KeySym symbol)
{
  Display* display = NULL;
  KeyCode keycode = 0U;

  if (app == NULL || app->display == NULL)
  {
    return 0;
  }

  display = (Display*)app->display;
  keycode = XKeysymToKeycode(display, symbol);
  if (keycode >= sizeof(app->key_down))
  {
    return 0;
  }

  return app->key_down[keycode] != 0U;
}

static void platform_sync_overlay_state(PlatformApp* app)
{
  if (app == NULL)
  {
    return;
  }

  app->overlay.cursor_mode_enabled = app->cursor_mode_enabled;
  app->overlay.hot_slider = OVERLAY_SLIDER_NONE;
  app->overlay.active_slider = OVERLAY_SLIDER_NONE;
  app->overlay.hot_toggle = OVERLAY_TOGGLE_NONE;
  app->overlay.hot_gpu_preference = -1;
  app->overlay.panel_width = 0;
  app->overlay.panel_collapsed = 1;
  app->overlay.mouse_x = 0;
  app->overlay.mouse_y = 0;
  app->overlay.scroll_offset = 0.0f;
  app->overlay.scroll_max = 0.0f;
}

static void platform_center_cursor(PlatformApp* app)
{
  Display* display = NULL;

  if (app == NULL || app->display == NULL || app->window == 0U || app->width <= 0 || app->height <= 0)
  {
    return;
  }

  display = (Display*)app->display;
  XWarpPointer(
    display,
    None,
    (Window)app->window,
    0,
    0,
    0U,
    0U,
    app->width / 2,
    app->height / 2);
  XFlush(display);
  app->suppress_next_mouse_delta = 1;
}

static void platform_set_mouse_capture(PlatformApp* app, int enabled)
{
  Display* display = NULL;
  Window window = 0U;

  if (app == NULL || app->display == NULL || app->window == 0U)
  {
    return;
  }

  display = (Display*)app->display;
  window = (Window)app->window;

  if (enabled != 0)
  {
    int grab_result = GrabSuccess;

    if (app->mouse_captured != 0)
    {
      return;
    }

    grab_result = XGrabPointer(
      display,
      window,
      True,
      ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      window,
      (Cursor)app->blank_cursor,
      CurrentTime);
    if (grab_result != GrabSuccess)
    {
      return;
    }

    if (app->blank_cursor != 0U)
    {
      XDefineCursor(display, window, (Cursor)app->blank_cursor);
    }
    app->mouse_dx = 0;
    app->mouse_dy = 0;
    app->mouse_captured = 1;
    platform_center_cursor(app);
    return;
  }

  if (app->mouse_captured != 0)
  {
    XUngrabPointer(display, CurrentTime);
    XUndefineCursor(display, window);
    XFlush(display);
    app->mouse_captured = 0;
  }
}

static void platform_toggle_fullscreen(PlatformApp* app)
{
  Display* display = NULL;
  Window window = 0U;
  XEvent event;

  if (app == NULL ||
    app->display == NULL ||
    app->window == 0U ||
    app->net_wm_state_atom == 0U ||
    app->net_wm_fullscreen_atom == 0U)
  {
    return;
  }

  display = (Display*)app->display;
  window = (Window)app->window;
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.display = display;
  event.xclient.window = window;
  event.xclient.message_type = (Atom)app->net_wm_state_atom;
  event.xclient.format = 32;
  event.xclient.data.l[0] = (app->fullscreen_enabled != 0) ? 0L : 1L;
  event.xclient.data.l[1] = (long)app->net_wm_fullscreen_atom;
  event.xclient.data.l[2] = 0L;
  event.xclient.data.l[3] = 1L;
  event.xclient.data.l[4] = 0L;
  XSendEvent(
    display,
    RootWindow(display, app->screen),
    False,
    SubstructureNotifyMask | SubstructureRedirectMask,
    &event);
  XFlush(display);
  app->fullscreen_enabled = (app->fullscreen_enabled == 0);
}

static Cursor platform_create_blank_cursor(Display* display, Window window)
{
  static const char k_empty_bitmap[] = { 0 };
  Pixmap bitmap = None;
  XColor color = { 0 };
  Cursor cursor = None;

  if (display == NULL || window == None)
  {
    return None;
  }

  bitmap = XCreateBitmapFromData(display, window, k_empty_bitmap, 1U, 1U);
  if (bitmap == None)
  {
    return None;
  }

  cursor = XCreatePixmapCursor(display, bitmap, bitmap, &color, &color, 0U, 0U);
  XFreePixmap(display, bitmap);
  return cursor;
}

int platform_create(PlatformApp* app, const char* title, int width, int height)
{
  static int visual_attributes[] = {
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_DEPTH_SIZE, 24,
    GLX_STENCIL_SIZE, 8,
    None
  };
  Display* display = NULL;
  int screen = 0;
  XVisualInfo* visual_info = NULL;
  Colormap colormap = 0U;
  XSetWindowAttributes window_attributes;
  Window window = 0U;
  GLXContext gl_context = NULL;
  Atom wm_delete_atom = None;

  if (app == NULL)
  {
    return 0;
  }

  memset(app, 0, sizeof(*app));
  app->requested_gpu_preference = GPU_PREFERENCE_MODE_AUTO;
  diagnostics_log("platform_create: x11 begin");

  display = XOpenDisplay(NULL);
  if (display == NULL)
  {
    platform_show_error_message("X11 Error", "Failed to open the X11 display.");
    return 0;
  }

  screen = DefaultScreen(display);
  visual_info = glXChooseVisual(display, screen, visual_attributes);
  if (visual_info == NULL)
  {
    platform_show_error_message("OpenGL Error", "Failed to choose an X11 visual for OpenGL.");
    XCloseDisplay(display);
    return 0;
  }

  colormap = XCreateColormap(display, RootWindow(display, screen), visual_info->visual, AllocNone);
  memset(&window_attributes, 0, sizeof(window_attributes));
  window_attributes.colormap = colormap;
  window_attributes.border_pixel = 0U;
  window_attributes.event_mask =
    ExposureMask |
    FocusChangeMask |
    StructureNotifyMask |
    KeyPressMask |
    KeyReleaseMask |
    ButtonPressMask |
    ButtonReleaseMask |
    PointerMotionMask;

  window = XCreateWindow(
    display,
    RootWindow(display, screen),
    0,
    0,
    (unsigned int)((width > 0) ? width : 1280),
    (unsigned int)((height > 0) ? height : 720),
    0U,
    visual_info->depth,
    InputOutput,
    visual_info->visual,
    CWBorderPixel | CWColormap | CWEventMask,
    &window_attributes);
  if (window == 0U)
  {
    platform_show_error_message("X11 Error", "Failed to create the X11 window.");
    XFreeColormap(display, colormap);
    XFree(visual_info);
    XCloseDisplay(display);
    return 0;
  }

  wm_delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
  (void)XSetWMProtocols(display, window, &wm_delete_atom, 1);
  XStoreName(display, window, (title != NULL) ? title : "OpenGL Sky");

  gl_context = glXCreateContext(display, visual_info, NULL, True);
  if (gl_context == NULL)
  {
    platform_show_error_message("OpenGL Error", "Failed to create the GLX context.");
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual_info);
    XCloseDisplay(display);
    return 0;
  }

  if (!glXMakeCurrent(display, window, gl_context))
  {
    platform_show_error_message("OpenGL Error", "Failed to activate the GLX context.");
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual_info);
    XCloseDisplay(display);
    return 0;
  }

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK)
  {
    platform_show_error_message("OpenGL Error", "Failed to load OpenGL entry points through GLEW.");
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual_info);
    XCloseDisplay(display);
    return 0;
  }
  (void)glGetError();

  XMapWindow(display, window);
  XFlush(display);

  app->display = display;
  app->gl_context = gl_context;
  app->window = (unsigned long)window;
  app->colormap = (unsigned long)colormap;
  app->wm_delete_atom = (unsigned long)wm_delete_atom;
  app->net_wm_state_atom = (unsigned long)XInternAtom(display, "_NET_WM_STATE", False);
  app->net_wm_fullscreen_atom = (unsigned long)XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  app->blank_cursor = (unsigned long)platform_create_blank_cursor(display, window);
  app->timer_start_seconds = platform_get_monotonic_seconds();
  app->screen = screen;
  app->width = (width > 0) ? width : 1280;
  app->height = (height > 0) ? height : 720;
  app->running = 1;
  app->resized = 1;
  app->overlay.settings = scene_settings_default();
  app->overlay.god_mode_enabled = 0;
  app->overlay.freeze_time_enabled = 0;
  platform_sync_overlay_state(app);
  platform_refresh_gpu_info(app);

  diagnostics_logf(
    "platform_create: success width=%d height=%d gl_version=%s renderer=%s vendor=%s",
    app->width,
    app->height,
    (const char*)glGetString(GL_VERSION),
    (const char*)glGetString(GL_RENDERER),
    (const char*)glGetString(GL_VENDOR));
  glViewport(0, 0, (app->width > 0) ? app->width : 1, (app->height > 0) ? app->height : 1);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glXSwapBuffers(display, window);

  XFree(visual_info);
  return 1;
}

void platform_destroy(PlatformApp* app)
{
  Display* display = NULL;

  if (app == NULL || app->display == NULL)
  {
    return;
  }

  diagnostics_log("platform_destroy: x11 begin");
  display = (Display*)app->display;
  app->running = 0;
  platform_set_mouse_capture(app, 0);

  if (app->blank_cursor != 0U)
  {
    XFreeCursor(display, (Cursor)app->blank_cursor);
    app->blank_cursor = 0U;
  }
  if (app->gl_context != NULL)
  {
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, (GLXContext)app->gl_context);
    app->gl_context = NULL;
  }
  if (app->window != 0U)
  {
    XDestroyWindow(display, (Window)app->window);
    app->window = 0U;
  }
  if (app->colormap != 0U)
  {
    XFreeColormap(display, (Colormap)app->colormap);
    app->colormap = 0U;
  }

  XCloseDisplay(display);
  app->display = NULL;
  diagnostics_log("platform_destroy: x11 end");
}

void platform_pump_messages(PlatformApp* app, PlatformInput* input)
{
  Display* display = NULL;
  int has_focus = 0;
  int alt_down = 0;
  int fullscreen_down = 0;
  int player_mode_down = 0;
  int toggle_cycle_down = 0;
  int reset_cycle_down = 0;
  int increase_speed_down = 0;
  int decrease_speed_down = 0;
  int move_forward_down = 0;
  int move_backward_down = 0;
  int move_left_down = 0;
  int move_right_down = 0;
  int jump_down = 0;
  int move_down_down = 0;
  int fast_modifier_down = 0;

  if (app == NULL || input == NULL || app->display == NULL)
  {
    return;
  }

  display = (Display*)app->display;
  memset(input, 0, sizeof(*input));
  input->selected_block_slot = -1;

  while (XPending(display) > 0)
  {
    XEvent event;

    memset(&event, 0, sizeof(event));
    XNextEvent(display, &event);
    switch (event.type)
    {
      case ClientMessage:
        if ((unsigned long)event.xclient.data.l[0] == app->wm_delete_atom)
        {
          app->running = 0;
        }
        break;

      case ConfigureNotify:
        if (event.xconfigure.width != app->width || event.xconfigure.height != app->height)
        {
          app->width = event.xconfigure.width;
          app->height = event.xconfigure.height;
          app->resized = 1;
        }
        break;

      case FocusIn:
        app->has_focus = 1;
        break;

      case FocusOut:
        app->has_focus = 0;
        break;

      case KeyPress:
        if (event.xkey.keycode < sizeof(app->key_down))
        {
          app->key_down[event.xkey.keycode] = 1U;
        }
        if (XLookupKeysym(&event.xkey, 0) == XK_Escape)
        {
          app->escape_requested = 1;
        }
        break;

      case KeyRelease:
        if (XEventsQueued(display, QueuedAfterReading) > 0)
        {
          XEvent next_event;

          XPeekEvent(display, &next_event);
          if (next_event.type == KeyPress &&
            next_event.xkey.keycode == event.xkey.keycode &&
            next_event.xkey.time == event.xkey.time)
          {
            break;
          }
        }
        if (event.xkey.keycode < sizeof(app->key_down))
        {
          app->key_down[event.xkey.keycode] = 0U;
        }
        break;

      case ButtonPress:
        if (event.xbutton.button == Button1)
        {
          app->left_button_down = 1;
        }
        else if (event.xbutton.button == Button3)
        {
          app->right_button_down = 1;
        }
        break;

      case ButtonRelease:
        if (event.xbutton.button == Button1)
        {
          app->left_button_down = 0;
        }
        else if (event.xbutton.button == Button3)
        {
          app->right_button_down = 0;
        }
        break;

      case MotionNotify:
        if (app->mouse_captured != 0 && app->cursor_mode_enabled == 0)
        {
          const int center_x = app->width / 2;
          const int center_y = app->height / 2;
          const int delta_x = event.xmotion.x - center_x;
          const int delta_y = center_y - event.xmotion.y;

          if (app->suppress_next_mouse_delta != 0 && delta_x == 0 && delta_y == 0)
          {
            app->suppress_next_mouse_delta = 0;
          }
          else
          {
            app->mouse_dx += delta_x;
            app->mouse_dy += delta_y;
          }
        }
        break;

      default:
        break;
    }
  }

  has_focus = platform_window_has_focus(app);
  alt_down = has_focus ? (platform_is_key_down(app, XK_Alt_L) || platform_is_key_down(app, XK_Alt_R)) : 0;
  fullscreen_down = has_focus ? platform_is_key_down(app, XK_F11) : 0;
  player_mode_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_g) : 0;
  toggle_cycle_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_p) : 0;
  reset_cycle_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_r) : 0;
  increase_speed_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_Up) : 0;
  decrease_speed_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_Down) : 0;
  move_forward_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_w) : 0;
  move_backward_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_s) : 0;
  move_left_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_a) : 0;
  move_right_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_d) : 0;
  jump_down = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_space) : 0;
  move_down_down = (has_focus && app->cursor_mode_enabled == 0) ?
    (platform_is_key_down(app, XK_Shift_L) || platform_is_key_down(app, XK_Shift_R)) : 0;
  fast_modifier_down = (has_focus && app->cursor_mode_enabled == 0) ?
    (platform_is_key_down(app, XK_Control_L) || platform_is_key_down(app, XK_Control_R)) : 0;
  platform_sync_overlay_state(app);

  if (has_focus && alt_down && !app->previous_alt_down)
  {
    app->cursor_mode_enabled = (app->cursor_mode_enabled == 0);
    if (app->cursor_mode_enabled != 0)
    {
      platform_set_mouse_capture(app, 0);
    }
    else
    {
      platform_set_mouse_capture(app, 1);
    }
    platform_sync_overlay_state(app);
  }

  if (!has_focus)
  {
    platform_set_mouse_capture(app, 0);
  }
  else if (app->cursor_mode_enabled == 0 && app->width > 0 && app->height > 0)
  {
    platform_set_mouse_capture(app, 1);
  }

  if (has_focus && fullscreen_down && !app->previous_fullscreen_down)
  {
    platform_toggle_fullscreen(app);
  }

  input->look_x = (float)app->mouse_dx;
  input->look_y = (float)app->mouse_dy;
  if (app->cursor_mode_enabled != 0)
  {
    input->look_x = 0.0f;
    input->look_y = 0.0f;
  }

  input->move_forward = (float)(move_forward_down - move_backward_down);
  input->move_right = (float)(move_right_down - move_left_down);
  input->escape_pressed = app->escape_requested;
  input->toggle_player_mode_pressed = player_mode_down && !app->previous_player_mode_down;
  input->toggle_cycle_pressed = toggle_cycle_down && !app->previous_toggle_cycle_down;
  input->reset_cycle_pressed = reset_cycle_down && !app->previous_reset_cycle_down;
  input->increase_cycle_speed_pressed = increase_speed_down && !app->previous_increase_speed_down;
  input->decrease_cycle_speed_pressed = decrease_speed_down && !app->previous_decrease_speed_down;
  input->scrub_backward_held = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_Left) : 0;
  input->scrub_forward_held = (has_focus && app->cursor_mode_enabled == 0) ? platform_is_key_down(app, XK_Right) : 0;
  input->scrub_fast_held = fast_modifier_down;
  input->move_fast_held = (app->overlay.god_mode_enabled != 0) ? fast_modifier_down : move_down_down;
  input->crouch_held = (app->overlay.god_mode_enabled == 0) ? fast_modifier_down : 0;
  input->jump_pressed = jump_down && !app->previous_jump_down;
  input->jump_held = jump_down;
  input->move_down_held = (app->overlay.god_mode_enabled != 0) ? move_down_down : 0;
  input->remove_block_pressed = (has_focus && app->cursor_mode_enabled == 0) ?
    (app->left_button_down && !app->previous_world_left_button_down) : 0;
  input->place_block_pressed = (has_focus && app->cursor_mode_enabled == 0) ?
    (app->right_button_down && !app->previous_world_right_button_down) : 0;

  if (has_focus && app->cursor_mode_enabled == 0)
  {
    if (platform_is_key_down(app, XK_1))
    {
      input->selected_block_slot = 0;
    }
    else if (platform_is_key_down(app, XK_2))
    {
      input->selected_block_slot = 1;
    }
    else if (platform_is_key_down(app, XK_3))
    {
      input->selected_block_slot = 2;
    }
    else if (platform_is_key_down(app, XK_4))
    {
      input->selected_block_slot = 3;
    }
  }

  if (app->cursor_mode_enabled != 0)
  {
    input->move_forward = 0.0f;
    input->move_right = 0.0f;
    input->toggle_player_mode_pressed = 0;
    input->toggle_cycle_pressed = 0;
    input->reset_cycle_pressed = 0;
    input->increase_cycle_speed_pressed = 0;
    input->decrease_cycle_speed_pressed = 0;
    input->scrub_backward_held = 0;
    input->scrub_forward_held = 0;
    input->scrub_fast_held = 0;
    input->move_fast_held = 0;
    input->crouch_held = 0;
    input->jump_pressed = 0;
    input->jump_held = 0;
    input->move_down_held = 0;
    input->remove_block_pressed = 0;
    input->place_block_pressed = 0;
    input->selected_block_slot = -1;
  }

  if (has_focus && app->mouse_captured != 0 && app->cursor_mode_enabled == 0)
  {
    platform_center_cursor(app);
  }

  app->mouse_dx = 0;
  app->mouse_dy = 0;
  app->escape_requested = 0;
  app->previous_player_mode_down = player_mode_down;
  app->previous_toggle_cycle_down = toggle_cycle_down;
  app->previous_reset_cycle_down = reset_cycle_down;
  app->previous_increase_speed_down = increase_speed_down;
  app->previous_decrease_speed_down = decrease_speed_down;
  app->previous_jump_down = jump_down;
  app->previous_alt_down = alt_down;
  app->previous_fullscreen_down = fullscreen_down;
  app->previous_world_left_button_down = app->left_button_down;
  app->previous_world_right_button_down = app->right_button_down;
}

void platform_request_close(PlatformApp* app)
{
  if (app != NULL)
  {
    app->running = 0;
  }
}

float platform_get_time_seconds(const PlatformApp* app)
{
  if (app == NULL)
  {
    return 0.0f;
  }

  return (float)(platform_get_monotonic_seconds() - app->timer_start_seconds);
}

void platform_swap_buffers(const PlatformApp* app)
{
  if (app != NULL && app->display != NULL && app->window != 0U)
  {
    glXSwapBuffers((Display*)app->display, (Window)app->window);
  }
}

void platform_set_window_title(const PlatformApp* app, const char* title)
{
  if (app != NULL && app->display != NULL && app->window != 0U)
  {
    XStoreName((Display*)app->display, (Window)app->window, (title != NULL) ? title : "OpenGL Sky");
    XFlush((Display*)app->display);
  }
}

void platform_get_scene_settings(const PlatformApp* app, SceneSettings* out_settings)
{
  if (out_settings == NULL)
  {
    return;
  }

  if (app == NULL)
  {
    *out_settings = scene_settings_default();
    return;
  }

  *out_settings = app->overlay.settings;
}

void platform_set_scene_settings(PlatformApp* app, const SceneSettings* settings)
{
  if (app == NULL || settings == NULL)
  {
    return;
  }

  app->overlay.settings = *settings;
}

int platform_get_god_mode_enabled(const PlatformApp* app)
{
  return (app != NULL && app->overlay.god_mode_enabled != 0);
}

void platform_set_god_mode_enabled(PlatformApp* app, int enabled)
{
  if (app == NULL)
  {
    return;
  }

  app->overlay.god_mode_enabled = (enabled != 0);
}

void platform_update_overlay_metrics(PlatformApp* app, const OverlayMetrics* metrics)
{
  if (app == NULL || metrics == NULL)
  {
    return;
  }

  app->overlay.metrics = *metrics;
}

int platform_consume_gpu_switch_request(PlatformApp* app, GpuPreferenceMode* out_mode)
{
  if (app == NULL || app->gpu_switch_requested == 0)
  {
    return 0;
  }

  if (out_mode != NULL)
  {
    *out_mode = app->requested_gpu_preference;
  }

  app->gpu_switch_requested = 0;
  return 1;
}

void platform_refresh_gpu_info(PlatformApp* app)
{
  if (app == NULL)
  {
    return;
  }

  (void)gpu_preferences_query(&app->overlay.gpu_info);
  if (app->gl_context != NULL)
  {
    gpu_preferences_set_current_renderer(
      &app->overlay.gpu_info,
      (const char*)glGetString(GL_RENDERER),
      (const char*)glGetString(GL_VENDOR));
  }
}

void platform_show_error_message(const char* title, const char* message)
{
  const char* safe_title = (title != NULL) ? title : "Error";
  const char* safe_message = (message != NULL) ? message : "Unknown error";

  diagnostics_logf("%s: %s", safe_title, safe_message);
  (void)fprintf(stderr, "%s: %s\n", safe_title, safe_message);
}
