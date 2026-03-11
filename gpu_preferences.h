#ifndef GPU_PREFERENCES_H
#define GPU_PREFERENCES_H

#define GPU_PREFERENCES_MAX_ADAPTERS 8
#define GPU_PREFERENCES_MAX_NAME_LENGTH 96
#define GPU_PREFERENCES_MAX_RENDERER_LENGTH 128

typedef enum GpuPreferenceMode
{
  GPU_PREFERENCE_MODE_AUTO = 0,
  GPU_PREFERENCE_MODE_MINIMUM_POWER,
  GPU_PREFERENCE_MODE_HIGH_PERFORMANCE,
  GPU_PREFERENCE_MODE_COUNT
} GpuPreferenceMode;

typedef struct GpuAdapterInfo
{
  char name[GPU_PREFERENCES_MAX_NAME_LENGTH];
  unsigned int dedicated_video_memory_mb;
  int is_minimum_power_candidate;
  int is_high_performance_candidate;
} GpuAdapterInfo;

typedef struct GpuPreferenceInfo
{
  int available;
  int adapter_count;
  int total_adapter_count;
  int minimum_power_index;
  int high_performance_index;
  GpuPreferenceMode selected_mode;
  char current_renderer[GPU_PREFERENCES_MAX_RENDERER_LENGTH];
  char current_vendor[GPU_PREFERENCES_MAX_NAME_LENGTH];
  char status_message[GPU_PREFERENCES_MAX_RENDERER_LENGTH];
  GpuAdapterInfo adapters[GPU_PREFERENCES_MAX_ADAPTERS];
} GpuPreferenceInfo;

int gpu_preferences_query(GpuPreferenceInfo* out_info);
void gpu_preferences_set_current_renderer(GpuPreferenceInfo* info, const char* renderer_name, const char* vendor_name);
int gpu_preferences_apply_and_relaunch(GpuPreferenceMode mode);
const char* gpu_preferences_get_mode_label(GpuPreferenceMode mode);
const char* gpu_preferences_get_mode_short_label(GpuPreferenceMode mode);

#endif
