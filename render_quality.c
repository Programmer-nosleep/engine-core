#include "render_quality.h"

#include "gpu_preferences.h"

#include <ctype.h>
#include <string.h>

static int render_quality_contains_case_insensitive(const char* text, const char* needle);
static unsigned int render_quality_query_dedicated_memory_mb(const char* renderer_name, const char* vendor_name);
static int render_quality_is_vendor_match(const char* adapter_name, const char* vendor_name);

RendererQualityProfile render_quality_pick(const char* renderer_name, const char* vendor_name)
{
  const unsigned int dedicated_memory_mb = render_quality_query_dedicated_memory_mb(renderer_name, vendor_name);
  RendererQualityProfile profile = {
    "Balanced",
    0.94f,
    0.70f,
    180.0f,
    1536,
    321,
    0,
    1,
    0,
    1,
    1
  };

  if (render_quality_contains_case_insensitive(renderer_name, "intel") ||
    render_quality_contains_case_insensitive(renderer_name, "iris") ||
    render_quality_contains_case_insensitive(renderer_name, "uhd") ||
    render_quality_contains_case_insensitive(vendor_name, "intel"))
  {
    profile.name = "iGPU";
    profile.render_scale = 0.80f;
    profile.trace_distance_scale = 0.40f;
    profile.shadow_extent = 220.0f;
    profile.shadow_map_size = 1024;
    profile.terrain_resolution = 257;
    profile.shadow_terrain_resolution = 0;
    profile.enable_raytrace = 0;
    profile.enable_pathtrace = 0;
    profile.enable_post_ao = 0;
    profile.enable_full_clouds = 0;
  }
  else if (dedicated_memory_mb > 0U && dedicated_memory_mb <= 4096U)
  {
    profile.name = "Hybrid";
    profile.render_scale = 0.90f;
    profile.trace_distance_scale = 0.48f;
    profile.shadow_extent = 200.0f;
    profile.shadow_map_size = 1024;
    profile.terrain_resolution = 257;
    profile.shadow_terrain_resolution = 0;
    profile.enable_raytrace = 0;
    profile.enable_pathtrace = 0;
    profile.enable_post_ao = 0;
    profile.enable_full_clouds = 0;
  }

  return profile;
}

static int render_quality_contains_case_insensitive(const char* text, const char* needle)
{
  size_t text_length = 0U;
  size_t needle_length = 0U;
  size_t i = 0U;
  size_t j = 0U;

  if (text == NULL || needle == NULL)
  {
    return 0;
  }

  text_length = strlen(text);
  needle_length = strlen(needle);
  if (needle_length == 0U || needle_length > text_length)
  {
    return 0;
  }

  for (i = 0U; i + needle_length <= text_length; ++i)
  {
    for (j = 0U; j < needle_length; ++j)
    {
      if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)needle[j]))
      {
        break;
      }
    }

    if (j == needle_length)
    {
      return 1;
    }
  }

  return 0;
}

static int render_quality_is_vendor_match(const char* adapter_name, const char* vendor_name)
{
  if (adapter_name == NULL || vendor_name == NULL)
  {
    return 0;
  }

  return render_quality_contains_case_insensitive(adapter_name, vendor_name) ||
    (render_quality_contains_case_insensitive(vendor_name, "nvidia") && render_quality_contains_case_insensitive(adapter_name, "nvidia")) ||
    (render_quality_contains_case_insensitive(vendor_name, "amd") && render_quality_contains_case_insensitive(adapter_name, "radeon")) ||
    (render_quality_contains_case_insensitive(vendor_name, "ati") && render_quality_contains_case_insensitive(adapter_name, "radeon")) ||
    (render_quality_contains_case_insensitive(vendor_name, "intel") && render_quality_contains_case_insensitive(adapter_name, "intel"));
}

static unsigned int render_quality_query_dedicated_memory_mb(const char* renderer_name, const char* vendor_name)
{
  GpuPreferenceInfo gpu_info = { 0 };
  int index = 0;
  int best_score = -1;
  unsigned int best_memory_mb = 0U;

  if (!gpu_preferences_query(&gpu_info))
  {
    return 0U;
  }

  for (index = 0; index < gpu_info.adapter_count; ++index)
  {
    const GpuAdapterInfo* adapter = &gpu_info.adapters[index];
    int score = 0;

    if (adapter->name[0] == '\0')
    {
      continue;
    }

    if (renderer_name != NULL &&
      (render_quality_contains_case_insensitive(renderer_name, adapter->name) ||
        render_quality_contains_case_insensitive(adapter->name, renderer_name)))
    {
      score += 8;
    }
    if (render_quality_is_vendor_match(adapter->name, vendor_name))
    {
      score += 2;
    }
    if (adapter->is_high_performance_candidate != 0 &&
      renderer_name != NULL &&
      (render_quality_contains_case_insensitive(renderer_name, "nvidia") ||
        render_quality_contains_case_insensitive(renderer_name, "radeon") ||
        render_quality_contains_case_insensitive(renderer_name, "geforce")))
    {
      score += 1;
    }

    if (score > best_score || (score == best_score && adapter->dedicated_video_memory_mb > best_memory_mb))
    {
      best_score = score;
      best_memory_mb = adapter->dedicated_video_memory_mb;
    }
  }

  if (best_score <= 0 && gpu_info.adapter_count == 1)
  {
    return gpu_info.adapters[0].dedicated_video_memory_mb;
  }

  return best_memory_mb;
}
