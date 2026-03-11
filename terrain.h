#ifndef TERRAIN_H
#define TERRAIN_H

#include "scene_settings.h"

typedef struct TerrainRenderSamplingConfig
{
  float origin_x;
  float origin_z;
  float mesh_step;
  float half_extent;
  int valid;
} TerrainRenderSamplingConfig;

float terrain_get_height(float x, float z, const SceneSettings* settings);
void terrain_set_render_sampling(const TerrainRenderSamplingConfig* config);
float terrain_get_render_height(float x, float z, const SceneSettings* settings);

#endif
