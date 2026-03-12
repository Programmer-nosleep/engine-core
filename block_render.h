#ifndef BLOCK_RENDER_H
#define BLOCK_RENDER_H

#include "atmosphere.h"
#include "block_world.h"
#include "camera.h"
#include "scene_settings.h"

static float block_render_clamp(float value, float min_value, float max_value);
static float block_render_mix(float a, float b, float t);
static void block_render_emit_face(int face_index, float x, float y, float z);
static void block_render_draw_wire_cube(float x, float y, float z, float r, float g, float b);

void block_render_draw_world(
  int width,
  int height,
  const CameraState* camera,
  const AtmosphereState* atmosphere,
  const SceneSettings* settings,
  const BlockWorld* world
);

#endif
