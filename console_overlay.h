#ifndef CONSOLE_OVERLAY_H
#define CONSOLE_OVERLAY_H

#include "gl_headers.h"
#include "overlay_ui.h"

typedef struct ConsoleOverlay
{
  GLuint font_base;
  GLuint backdrop_texture;
  GLuint backdrop_program;
  GLuint backdrop_vao;
  GLuint backdrop_vertex_buffer;
  int backdrop_width;
  int backdrop_height;
  GLint backdrop_viewport_size_location;
  GLint backdrop_texel_size_location;
  GLint backdrop_blur_radius_location;
  GLint backdrop_sampler_location;
} ConsoleOverlay;

int console_overlay_create(ConsoleOverlay* overlay);
void console_overlay_destroy(ConsoleOverlay* overlay);
void console_overlay_render(ConsoleOverlay* overlay, int width, int height, const OverlayState* state);

#endif
