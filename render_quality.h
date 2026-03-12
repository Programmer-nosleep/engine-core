#ifndef RENDER_QUALITY_H
#define RENDER_QUALITY_H

typedef struct RendererQualityProfile
{
  const char* name;
  float render_scale;
  float trace_distance_scale;
  float shadow_extent;
  int shadow_map_size;
  int terrain_resolution;
  int shadow_terrain_resolution;
  int enable_raytrace;
  int enable_pathtrace;
  int enable_post_ao;
  int enable_full_clouds;
  int shadow_update_interval;
  int enable_grass_shadows;
  float tree_density_scale;
  float grass_density_scale;
} RendererQualityProfile;

RendererQualityProfile render_quality_pick(const char* renderer_name, const char* vendor_name);

#endif
