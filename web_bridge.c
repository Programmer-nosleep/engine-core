#include "block_world.h"
#include "scene_settings.h"
#include "terrain.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define WEB_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WEB_EXPORT
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
  ENGINE_WEB_BLOCK_STRIDE_FLOATS = 6
};

typedef struct EngineWebState
{
  SceneSettings settings;
  BlockWorld world;
  int initialized;
} EngineWebState;

static EngineWebState g_engine_web = { 0 };

static float engine_web_clampf(float value, float minimum, float maximum);
static void engine_web_initialize(void);
static int engine_web_get_support_height(int x, int z, const SceneSettings* settings);
static BlockType engine_web_resolve_block_type(int block_type);
static float engine_web_sample_world_x(int column, int width, float center_x, float step);
static float engine_web_sample_world_z(int row, int height, float center_z, float step);
static char* engine_web_duplicate_string(const char* text);

WEB_EXPORT void engine_web_reset_state(void)
{
  memset(&g_engine_web, 0, sizeof(g_engine_web));
  engine_web_initialize();
}

WEB_EXPORT void engine_web_set_terrain_profile(
  float base_height,
  float height_scale,
  float roughness,
  float ridge_strength
)
{
  engine_web_initialize();

  g_engine_web.settings.terrain_base_height = engine_web_clampf(base_height, -48.0f, 48.0f);
  g_engine_web.settings.terrain_height_scale = engine_web_clampf(height_scale, 0.25f, 4.0f);
  g_engine_web.settings.terrain_roughness = engine_web_clampf(roughness, 0.25f, 3.0f);
  g_engine_web.settings.terrain_ridge_strength = engine_web_clampf(ridge_strength, 0.0f, 3.0f);
  block_world_refresh(&g_engine_web.world, &g_engine_web.settings);
}

WEB_EXPORT int engine_web_fill_height_buffer(
  int width,
  int height,
  float center_x,
  float center_z,
  float step,
  float* out_heights
)
{
  int row = 0;
  int column = 0;

  if (width <= 0 || height <= 0 || step <= 0.0f || out_heights == NULL)
  {
    return 0;
  }

  engine_web_initialize();

  for (row = 0; row < height; ++row)
  {
    for (column = 0; column < width; ++column)
    {
      const float world_x = engine_web_sample_world_x(column, width, center_x, step);
      const float world_z = engine_web_sample_world_z(row, height, center_z, step);
      out_heights[row * width + column] = terrain_get_height(world_x, world_z, &g_engine_web.settings);
    }
  }

  return width * height;
}

WEB_EXPORT int engine_web_get_block_count(void)
{
  engine_web_initialize();
  return block_world_get_cell_count(&g_engine_web.world);
}

WEB_EXPORT int engine_web_copy_blocks(float* out_block_data, int max_blocks)
{
  const BlockWorldCell* cells = NULL;
  int cell_count = 0;
  int copy_count = 0;
  int index = 0;

  if (out_block_data == NULL || max_blocks <= 0)
  {
    return 0;
  }

  engine_web_initialize();
  cells = block_world_get_cells(&g_engine_web.world, &cell_count);
  if (cells == NULL || cell_count <= 0)
  {
    return 0;
  }

  copy_count = (cell_count < max_blocks) ? cell_count : max_blocks;
  for (index = 0; index < copy_count; ++index)
  {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    const int base_index = index * ENGINE_WEB_BLOCK_STRIDE_FLOATS;

    block_world_get_block_color(cells[index].type, &r, &g, &b);
    out_block_data[base_index + 0] = (float)cells[index].x + 0.5f;
    out_block_data[base_index + 1] = (float)cells[index].y + 0.5f;
    out_block_data[base_index + 2] = (float)cells[index].z + 0.5f;
    out_block_data[base_index + 3] = r;
    out_block_data[base_index + 4] = g;
    out_block_data[base_index + 5] = b;
  }

  return copy_count;
}

WEB_EXPORT int engine_web_place_block_column(int x, int z, int block_type)
{
  const BlockWorldCell* cells = NULL;
  BlockType resolved_type = BLOCK_TYPE_GRASS;
  int cell_count = 0;
  int highest_y = 0;
  int index = 0;

  engine_web_initialize();
  highest_y = engine_web_get_support_height(x, z, &g_engine_web.settings) - 1;
  resolved_type = engine_web_resolve_block_type(block_type);
  cells = block_world_get_cells(&g_engine_web.world, &cell_count);
  for (index = 0; index < cell_count; ++index)
  {
    if (cells[index].x == x && cells[index].z == z && cells[index].y > highest_y)
    {
      highest_y = cells[index].y;
    }
  }

  return block_world_place_block(&g_engine_web.world, x, highest_y + 1, z, resolved_type, &g_engine_web.settings);
}

WEB_EXPORT int engine_web_remove_block_column(int x, int z)
{
  const BlockWorldCell* cells = NULL;
  int cell_count = 0;
  int highest_y = 0;
  int found = 0;
  int index = 0;

  engine_web_initialize();
  cells = block_world_get_cells(&g_engine_web.world, &cell_count);
  for (index = 0; index < cell_count; ++index)
  {
    if (cells[index].x == x && cells[index].z == z && (!found || cells[index].y > highest_y))
    {
      highest_y = cells[index].y;
      found = 1;
    }
  }

  if (!found)
  {
    return 0;
  }

  return block_world_remove_block(&g_engine_web.world, x, highest_y, z);
}

WEB_EXPORT char* engine_web_describe_state(void)
{
  const BlockWorldCell* cells = NULL;
  char highest_block_text[32] = "n/a";
  char summary[256] = { 0 };
  int cell_count = 0;
  int index = 0;
  int highest_block_y = 0;
  int has_highest_block = 0;
  float center_height = 0.0f;

  engine_web_initialize();
  center_height = terrain_get_height(0.0f, 0.0f, &g_engine_web.settings);
  cells = block_world_get_cells(&g_engine_web.world, &cell_count);
  for (index = 0; index < cell_count; ++index)
  {
    if (!has_highest_block || cells[index].y > highest_block_y)
    {
      highest_block_y = cells[index].y;
      has_highest_block = 1;
    }
  }

  if (has_highest_block)
  {
    (void)snprintf(highest_block_text, sizeof(highest_block_text), "%d", highest_block_y);
  }

  (void)snprintf(
    summary,
    sizeof(summary),
    "base %.1f | scale %.2f | rough %.2f | ridge %.2f | blocks %d | terrain@0,0 %.1f | top block %s",
    g_engine_web.settings.terrain_base_height,
    g_engine_web.settings.terrain_height_scale,
    g_engine_web.settings.terrain_roughness,
    g_engine_web.settings.terrain_ridge_strength,
    cell_count,
    center_height,
    highest_block_text
  );

  return engine_web_duplicate_string(summary);
}

static float engine_web_clampf(float value, float minimum, float maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }

  return value;
}

static void engine_web_initialize(void)
{
  if (g_engine_web.initialized != 0)
  {
    return;
  }

  g_engine_web.settings = scene_settings_default();
  block_world_init(&g_engine_web.world, &g_engine_web.settings);
  g_engine_web.initialized = 1;
}

static int engine_web_get_support_height(int x, int z, const SceneSettings* settings)
{
  const float sample_offsets[9][2] = {
    { 0.15f, 0.15f },
    { 0.50f, 0.15f },
    { 0.85f, 0.15f },
    { 0.15f, 0.50f },
    { 0.50f, 0.50f },
    { 0.85f, 0.50f },
    { 0.15f, 0.85f },
    { 0.50f, 0.85f },
    { 0.85f, 0.85f }
  };
  float highest_terrain = terrain_get_height((float)x + 0.5f, (float)z + 0.5f, settings);
  int sample_index = 0;

  for (sample_index = 0; sample_index < 9; ++sample_index)
  {
    const float sample_height = terrain_get_height((float)x + sample_offsets[sample_index][0], (float)z + sample_offsets[sample_index][1], settings);
    if (sample_height > highest_terrain)
    {
      highest_terrain = sample_height;
    }
  }

  return (int)ceilf(highest_terrain - 0.001f);
}

static BlockType engine_web_resolve_block_type(int block_type)
{
  if (block_type <= (int)BLOCK_TYPE_NONE || block_type >= (int)BLOCK_TYPE_COUNT)
  {
    return BLOCK_TYPE_GRASS;
  }

  return (BlockType)block_type;
}

static float engine_web_sample_world_x(int column, int width, float center_x, float step)
{
  return center_x + ((float)column - (float)(width - 1) * 0.5f) * step;
}

static float engine_web_sample_world_z(int row, int height, float center_z, float step)
{
  return center_z + ((float)row - (float)(height - 1) * 0.5f) * step;
}

static char* engine_web_duplicate_string(const char* text)
{
  char* result = NULL;
  size_t length = 0U;

  if (text == NULL)
  {
    return NULL;
  }

  length = strlen(text);
  result = (char*)malloc(length + 1U);
  if (result == NULL)
  {
    return NULL;
  }

  memcpy(result, text, length + 1U);
  return result;
}
