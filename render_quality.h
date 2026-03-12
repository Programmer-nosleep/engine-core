#ifndef RENDER_QUALITY_H
#define RENDER_QUALITY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RendererQualityPreset
{
  RENDER_QUALITY_PRESET_HIGH = 0,
  RENDER_QUALITY_PRESET_LOW,
  RENDER_QUALITY_PRESET_ULTRA_LOW,
  RENDER_QUALITY_PRESET_COUNT
} RendererQualityPreset;

typedef struct RendererQualityProfile
{
  const char* name;
  RendererQualityPreset preset;
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

RendererQualityPreset render_quality_pick_preset(const char* renderer_name, const char* vendor_name);
RendererQualityProfile render_quality_pick(const char* renderer_name, const char* vendor_name);
RendererQualityProfile render_quality_get_profile(RendererQualityPreset preset, const char* renderer_name, const char* vendor_name);
const char* render_quality_preset_get_label(RendererQualityPreset preset);
const char* render_quality_preset_get_description(RendererQualityPreset preset);

#ifdef __cplusplus
}
#endif

#endif
