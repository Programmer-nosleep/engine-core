#ifndef BLOCK_RENDER_H
#define BLOCK_RENDER_H

#include "atmosphere.h"
#include "block_world.h"
#include "camera.h"
#include "scene_settings.h"

void block_render_draw_world(
  int width,
  int height,
  const CameraState* camera,
  const AtmosphereState* atmosphere,
  const SceneSettings* settings,
  const BlockWorld* world
);

#endif
