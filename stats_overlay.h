#ifndef STATS_OVERLAY_H
#define STATS_OVERLAY_H

#include "gl_headers.h"
#include "overlay_ui.h"

typedef struct StatsOverlay
{
  GLuint small_font_base;
  GLuint large_font_base;
  GLuint hero_font_base;
  float frame_time_history[96];
  int frame_time_history_write_index;
  int frame_time_history_count;
  unsigned int last_frame_sample_index;
} StatsOverlay;

int stats_overlay_create(StatsOverlay* overlay);
void stats_overlay_destroy(StatsOverlay* overlay);
void stats_overlay_render(StatsOverlay* overlay, int width, int height, const OverlayState* state);

#endif
