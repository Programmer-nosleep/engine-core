#ifndef SCENE_SETTINGS_H
#define SCENE_SETTINGS_H

typedef struct SceneSettings
{
  float sun_distance_mkm;
  float sun_orbit_degrees;
  float cycle_duration_seconds;
  float daylight_fraction;
  float camera_fov_degrees;
  float fog_density;
  float cloud_amount;
  float cloud_spacing;
  float terrain_base_height;
  float terrain_height_scale;
  float terrain_roughness;
  float terrain_ridge_strength;
  float palm_size;
  float palm_count;
  float palm_fruit_density;
  float palm_render_radius;
  int clouds_enabled;
} SceneSettings;

typedef struct OverlayMetrics
{
  float sun_distance_mkm;
  float daylight_duration_seconds;
  float night_duration_seconds;
  float frames_per_second;
  float frame_time_ms;
  float cpu_usage_percent;
  float gpu0_usage_percent;
  float gpu1_usage_percent;
  float player_position_x;
  float player_position_y;
  float player_position_z;
  int player_mode;
  int selected_block_type;
  int placed_block_count;
  int target_active;
  unsigned int stats_sample_index;
} OverlayMetrics;

static inline SceneSettings scene_settings_default(void)
{
  SceneSettings settings = {
    149.6f,
    82.8f,
    180.0f,
    0.5f,
    65.0f,
    0.24f,
    0.54f,
    1.0f,
    -14.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    24.0f,
    0.55f,
    260.0f,
    1
  };
  return settings;
}

#endif
