#include "palm_render.h"

#include "diagnostics.h"
#include "procedural_lod.h"
#include "terrain.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define palm_render_sscanf sscanf_s
#else
#define palm_render_sscanf sscanf
#endif

typedef struct PalmVertex
{
  float position[3];
  float normal[3];
  float color[3];
} PalmVertex;

typedef struct PalmColor
{
  float r;
  float g;
  float b;
} PalmColor;

typedef struct PalmVec3
{
  float x;
  float y;
  float z;
} PalmVec3;

typedef struct PalmInstanceData
{
  float transform[16];
  float tint[4];
} PalmInstanceData;

typedef struct PalmObjIndex
{
  int position_index;
  int normal_index;
} PalmObjIndex;

typedef struct PalmMaterial
{
  char name[64];
  PalmColor diffuse;
} PalmMaterial;

typedef struct PalmVec3Array
{
  PalmVec3* data;
  size_t count;
  size_t capacity;
} PalmVec3Array;

typedef struct PalmVertexArray
{
  PalmVertex* data;
  size_t count;
  size_t capacity;
} PalmVertexArray;

typedef struct PalmMaterialArray
{
  PalmMaterial* data;
  size_t count;
  size_t capacity;
} PalmMaterialArray;

enum
{
  PALM_RENDER_MAX_FACE_VERTICES = 32
};

static const float k_palm_render_pi = 3.14159265f;
static const char* k_palm_render_obj_paths[PALM_RENDER_MAX_VARIANTS] = {
  "res/obj/kelapasawit.obj",
  "res/obj/Date Palm.obj",
  "res/obj/Tree.obj"
};

static void palm_render_show_error(const char* title, const char* message);
static const char* palm_render_find_last_path_separator(const char* path);
static int palm_render_file_exists(const char* path);
static int palm_render_build_relative_path(const char* base_path, const char* relative_path, char* out_path, size_t out_path_size);
static int palm_render_resolve_asset_path(HINSTANCE instance, const char* relative_path, char* out_path, size_t out_path_size);
static int palm_render_load_text_file(const char* path, const char* label, char** out_text);
static char* palm_render_trim_left(char* text);
static int palm_render_reserve_memory(void** buffer, size_t* capacity, size_t required, size_t element_size);
static int palm_render_push_vec3(PalmVec3Array* array, PalmVec3 value);
static int palm_render_push_vertex(PalmVertexArray* array, PalmVertex value);
static PalmMaterial* palm_render_push_material(PalmMaterialArray* array, const char* name);
static int palm_render_parse_mtl(const char* mtl_path, PalmMaterialArray* materials);
static const PalmMaterial* palm_render_find_material(const PalmMaterialArray* materials, const char* name);
static int palm_render_parse_face_vertex(const char* token, PalmObjIndex* out_index);
static int palm_render_resolve_obj_index(int obj_index, size_t count);
static PalmVec3 palm_render_vec3_subtract(PalmVec3 a, PalmVec3 b);
static PalmVec3 palm_render_vec3_cross(PalmVec3 a, PalmVec3 b);
static float palm_render_vec3_dot(PalmVec3 a, PalmVec3 b);
static PalmVec3 palm_render_vec3_normalize(PalmVec3 value);
static int palm_render_load_model_vertices(HINSTANCE instance, const char* relative_obj_path, PalmVertex** out_vertices, GLsizei* out_vertex_count, float* out_model_height);
static int palm_render_create_variant(PalmRenderVariant* variant, HINSTANCE instance, const char* relative_obj_path);
static void palm_render_destroy_variant(PalmRenderVariant* variant);
static int palm_render_reserve_instances(PalmRenderVariant* variant, size_t required_instance_capacity);
static float palm_render_clamp(float value, float min_value, float max_value);
static float palm_render_mix(float a, float b, float t);
static float palm_render_hash_unit(int x, int z, unsigned int seed);
static float palm_render_estimate_slope(float x, float z, const SceneSettings* settings);
static GLsizei palm_render_get_max_vertex_count(const PalmRenderMesh* mesh);
static void palm_render_build_instance_transform(PalmInstanceData* instance, float x, float y, float z, float scale, float yaw_radians, PalmColor tint);

int palm_render_create(PalmRenderMesh* mesh, HINSTANCE instance)
{
  int variant_index = 0;

  if (mesh == NULL)
  {
    return 0;
  }

  memset(mesh, 0, sizeof(*mesh));

  for (variant_index = 0; variant_index < PALM_RENDER_MAX_VARIANTS; ++variant_index)
  {
    if (palm_render_create_variant(&mesh->variants[mesh->variant_count], instance, k_palm_render_obj_paths[variant_index]))
    {
      mesh->variant_count += 1;
    }
  }

  if (mesh->variant_count <= 0)
  {
    palm_render_destroy(mesh);
    palm_render_show_error("Palm Error", "Failed to load any palm OBJ variants.");
    return 0;
  }

  diagnostics_logf("palm_render: variant_count=%d", mesh->variant_count);
  return 1;
}

static int palm_render_create_variant(PalmRenderVariant* variant, HINSTANCE instance, const char* relative_obj_path)
{
  PalmVertex* vertices = NULL;
  GLsizei vertex_count = 0;
  float model_height = 1.0f;
  int column = 0;

  if (variant == NULL || relative_obj_path == NULL)
  {
    return 0;
  }

  memset(variant, 0, sizeof(*variant));
  if (!palm_render_load_model_vertices(instance, relative_obj_path, &vertices, &vertex_count, &model_height))
  {
    return 0;
  }

  glGenVertexArrays(1, &variant->vao);
  glGenBuffers(1, &variant->vertex_buffer);
  glGenBuffers(1, &variant->instance_buffer);
  if (variant->vao == 0U || variant->vertex_buffer == 0U || variant->instance_buffer == 0U)
  {
    free(vertices);
    palm_render_destroy_variant(variant);
    palm_render_show_error("Palm Error", "Failed to allocate palm render buffers.");
    return 0;
  }

  variant->vertex_count = vertex_count;
  variant->model_height = model_height;

  glBindVertexArray(variant->vao);

  glBindBuffer(GL_ARRAY_BUFFER, variant->vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(PalmVertex) * (size_t)variant->vertex_count, vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PalmVertex), (const void*)offsetof(PalmVertex, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PalmVertex), (const void*)offsetof(PalmVertex, normal));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(PalmVertex), (const void*)offsetof(PalmVertex, color));

  glBindBuffer(GL_ARRAY_BUFFER, variant->instance_buffer);
  glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
  for (column = 0; column < 4; ++column)
  {
    const GLuint attribute = (GLuint)(3 + column);
    const size_t offset = offsetof(PalmInstanceData, transform) + sizeof(float) * 4U * (size_t)column;
    glEnableVertexAttribArray(attribute);
    glVertexAttribPointer(attribute, 4, GL_FLOAT, GL_FALSE, sizeof(PalmInstanceData), (const void*)offset);
    glVertexAttribDivisor(attribute, 1);
  }
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(PalmInstanceData), (const void*)offsetof(PalmInstanceData, tint));
  glVertexAttribDivisor(7, 1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  free(vertices);
  return 1;
}

static void palm_render_destroy_variant(PalmRenderVariant* variant)
{
  if (variant == NULL)
  {
    return;
  }

  if (variant->instance_buffer != 0U)
  {
    glDeleteBuffers(1, &variant->instance_buffer);
    variant->instance_buffer = 0U;
  }
  if (variant->vertex_buffer != 0U)
  {
    glDeleteBuffers(1, &variant->vertex_buffer);
    variant->vertex_buffer = 0U;
  }
  if (variant->vao != 0U)
  {
    glDeleteVertexArrays(1, &variant->vao);
    variant->vao = 0U;
  }
  if (variant->cpu_instances != NULL)
  {
    free(variant->cpu_instances);
    variant->cpu_instances = NULL;
  }
  variant->cpu_instance_capacity = 0U;
  variant->vertex_count = 0;
  variant->instance_count = 0;
  variant->model_height = 0.0f;
}

void palm_render_destroy(PalmRenderMesh* mesh)
{
  int variant_index = 0;

  if (mesh == NULL)
  {
    return;
  }

  for (variant_index = 0; variant_index < PALM_RENDER_MAX_VARIANTS; ++variant_index)
  {
    palm_render_destroy_variant(&mesh->variants[variant_index]);
  }
  mesh->variant_count = 0;
}

int palm_render_update(
  PalmRenderMesh* mesh,
  const CameraState* camera,
  const SceneSettings* settings,
  const RendererQualityProfile* quality)
{
  const SceneSettings fallback_settings = scene_settings_default();
  const SceneSettings* active_settings = (settings != NULL) ? settings : &fallback_settings;
  ProceduralLodConfig lod_config;
  ProceduralLodState lod_state;
  const GLsizei max_vertex_count = palm_render_get_max_vertex_count(mesh);
  float radius = 0.0f;
  int effective_tree_target = 0;
  float cell_size = 0.0f;
  int grid_z = 0;
  int variant_index = 0;

  if (mesh == NULL)
  {
    return 0;
  }

  lod_config.requested_radius = active_settings->palm_render_radius;
  lod_config.requested_radius_min = 80.0f;
  lod_config.requested_radius_max = 900.0f;
  lod_config.radius_scale_low = 1.10f;
  lod_config.radius_scale_high = 1.28f;
  lod_config.effective_radius_min = 96.0f;
  lod_config.effective_radius_max = 1150.0f;
  lod_config.requested_instance_count = active_settings->palm_count;
  lod_config.requested_instance_count_min = 0.0f;
  lod_config.requested_instance_count_max = 4000.0f;
  lod_config.instance_budget_min = 36;
  lod_config.instance_budget_max = 96;
  lod_config.source_vertex_count = (max_vertex_count > 0) ? (float)max_vertex_count : 0.0f;
  lod_config.fallback_vertex_count = 92502.0f;
  lod_config.vertex_budget_low = 3600000.0f;
  lod_config.vertex_budget_high = 8200000.0f;
  lod_config.cell_size_min = 16.0f;
  lod_config.cell_size_max = 120.0f;

  lod_state = procedural_lod_resolve(quality, &lod_config);
  radius = lod_state.effective_radius;
  effective_tree_target = lod_state.effective_instance_count;
  cell_size = lod_state.cell_size;

  for (variant_index = 0; variant_index < mesh->variant_count; ++variant_index)
  {
    mesh->variants[variant_index].instance_count = 0;
  }

  if (camera == NULL || mesh->variant_count <= 0 || max_vertex_count <= 0 ||
    effective_tree_target <= 0 || active_settings->palm_size <= 0.01f || cell_size <= 0.0f)
  {
    return 1;
  }

  {
    const int grid_min_x = (int)floorf((camera->x - radius) / cell_size);
    const int grid_max_x = (int)ceilf((camera->x + radius) / cell_size);
    const int grid_min_z = (int)floorf((camera->z - radius) / cell_size);
    const int grid_max_z = (int)ceilf((camera->z + radius) / cell_size);
    const size_t estimated_capacity = (size_t)(grid_max_x - grid_min_x + 1) * (size_t)(grid_max_z - grid_min_z + 1);

    for (variant_index = 0; variant_index < mesh->variant_count; ++variant_index)
    {
      if (!palm_render_reserve_instances(&mesh->variants[variant_index], estimated_capacity))
      {
        return 0;
      }
    }

    for (grid_z = grid_min_z; grid_z <= grid_max_z; ++grid_z)
    {
      int grid_x = 0;

      for (grid_x = grid_min_x; grid_x <= grid_max_x; ++grid_x)
      {
        const float offset_x = palm_render_hash_unit(grid_x, grid_z, 0U);
        const float offset_z = palm_render_hash_unit(grid_x, grid_z, 1U);
        const float variation = palm_render_hash_unit(grid_x, grid_z, 2U);
        const float scale_jitter = palm_render_hash_unit(grid_x, grid_z, 3U);
        const float x = ((float)grid_x + offset_x) * cell_size;
        const float z = ((float)grid_z + offset_z) * cell_size;
        PalmRenderVariant* variant = NULL;
        const float dx = x - camera->x;
        const float dz = z - camera->z;
        const float distance_sq = dx * dx + dz * dz;
        const float slope = palm_render_estimate_slope(x, z, active_settings);
        const float desired_height = palm_render_mix(8.2f, 12.4f, variation) *
          active_settings->palm_size * palm_render_mix(0.82f, 1.22f, scale_jitter);
        const float yaw = palm_render_hash_unit(grid_x, grid_z, 4U) * (k_palm_render_pi * 2.0f);
        const float ground_y = terrain_get_height(x, z, active_settings);
        const float embed_depth = palm_render_mix(0.22f, 0.58f, variation) * active_settings->palm_size + slope * 0.18f;
        const float fruit_density = palm_render_clamp(active_settings->palm_fruit_density, 0.0f, 1.0f);
        PalmColor tint = {
          palm_render_mix(0.94f, 1.04f, variation) + fruit_density * 0.05f,
          palm_render_mix(0.92f, 1.10f, scale_jitter),
          palm_render_mix(0.92f, 1.03f, variation) - fruit_density * 0.03f
        };

        if (distance_sq > radius * radius || slope > 1.45f)
        {
          continue;
        }

        variant = &mesh->variants[(int)(palm_render_hash_unit(grid_x, grid_z, 5U) * (float)mesh->variant_count)];
        if (variant->model_height <= 0.0001f || variant->vao == 0U || variant->vertex_count <= 0)
        {
          continue;
        }

        if ((size_t)variant->instance_count >= variant->cpu_instance_capacity)
        {
          continue;
        }

        palm_render_build_instance_transform(
          &((PalmInstanceData*)variant->cpu_instances)[variant->instance_count],
          x,
          ground_y - embed_depth,
          z,
          desired_height / variant->model_height,
          yaw,
          tint);
        variant->instance_count += 1;
      }
    }
  }

  for (variant_index = 0; variant_index < mesh->variant_count; ++variant_index)
  {
    PalmRenderVariant* variant = &mesh->variants[variant_index];

    if (variant->instance_count <= 0)
    {
      continue;
    }

    glBindBuffer(GL_ARRAY_BUFFER, variant->instance_buffer);
    glBufferData(
      GL_ARRAY_BUFFER,
      sizeof(PalmInstanceData) * (size_t)variant->instance_count,
      variant->cpu_instances,
      GL_DYNAMIC_DRAW);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return 1;
}

void palm_render_draw(const PalmRenderMesh* mesh)
{
  int variant_index = 0;

  if (mesh == NULL || mesh->variant_count <= 0)
  {
    return;
  }

  for (variant_index = 0; variant_index < mesh->variant_count; ++variant_index)
  {
    const PalmRenderVariant* variant = &mesh->variants[variant_index];

    if (variant->vao == 0U || variant->vertex_count <= 0 || variant->instance_count <= 0)
    {
      continue;
    }

    glBindVertexArray(variant->vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, variant->vertex_count, variant->instance_count);
  }
  glBindVertexArray(0);
}

static void palm_render_show_error(const char* title, const char* message)
{
  diagnostics_logf("%s: %s", title, message);
  (void)MessageBoxA(NULL, message, title, MB_ICONERROR | MB_OK);
}

static const char* palm_render_find_last_path_separator(const char* path)
{
  const char* last_backslash = NULL;
  const char* last_slash = NULL;

  if (path == NULL)
  {
    return NULL;
  }

  last_backslash = strrchr(path, '\\');
  last_slash = strrchr(path, '/');
  if (last_backslash == NULL)
  {
    return last_slash;
  }
  if (last_slash == NULL)
  {
    return last_backslash;
  }

  return (last_backslash > last_slash) ? last_backslash : last_slash;
}

static int palm_render_file_exists(const char* path)
{
  DWORD attributes = 0U;

  if (path == NULL || path[0] == '\0')
  {
    return 0;
  }

  attributes = GetFileAttributesA(path);
  if (attributes == INVALID_FILE_ATTRIBUTES)
  {
    return 0;
  }

  return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U;
}

static int palm_render_build_relative_path(const char* base_path, const char* relative_path, char* out_path, size_t out_path_size)
{
  const char* last_separator = NULL;
  size_t directory_length = 0U;

  if (base_path == NULL || relative_path == NULL || out_path == NULL || out_path_size == 0U)
  {
    return 0;
  }

  last_separator = palm_render_find_last_path_separator(base_path);
  if (last_separator == NULL)
  {
    return 0;
  }

  directory_length = (size_t)(last_separator - base_path + 1);
  if (directory_length + strlen(relative_path) + 1U > out_path_size)
  {
    return 0;
  }

  memcpy(out_path, base_path, directory_length);
  (void)snprintf(out_path + directory_length, out_path_size - directory_length, "%s", relative_path);
  return 1;
}

static int palm_render_resolve_asset_path(HINSTANCE instance, const char* relative_path, char* out_path, size_t out_path_size)
{
  char module_path[MAX_PATH] = { 0 };
  char candidate_path[MAX_PATH] = { 0 };
  char current_directory[MAX_PATH] = { 0 };
  DWORD path_length = 0U;
  char* last_separator = NULL;
  size_t base_length = 0U;
  static const char* k_res_prefix = "res/";
  static const char* k_res_fallbacks[] = {
    "res/",
    "..\\res\\",
    "..\\..\\res\\",
    "..\\..\\..\\res\\"
  };
  size_t i = 0U;

  path_length = GetModuleFileNameA(instance, module_path, (DWORD)sizeof(module_path));
  if (path_length == 0U || path_length >= (DWORD)sizeof(module_path))
  {
    palm_render_show_error("Path Error", "Failed to resolve the executable directory for palm assets.");
    return 0;
  }

  last_separator = (char*)palm_render_find_last_path_separator(module_path);
  if (last_separator == NULL)
  {
    palm_render_show_error("Path Error", "Failed to resolve the executable directory separator for palm assets.");
    return 0;
  }

  last_separator[1] = '\0';
  if (strlen(module_path) + strlen(relative_path) + 1U <= sizeof(candidate_path))
  {
    (void)snprintf(candidate_path, sizeof(candidate_path), "%s%s", module_path, relative_path);
    if (palm_render_file_exists(candidate_path))
    {
      (void)snprintf(out_path, out_path_size, "%s", candidate_path);
      return 1;
    }
  }

  if (GetCurrentDirectoryA((DWORD)sizeof(current_directory), current_directory) > 0U)
  {
    base_length = strlen(current_directory);
    if (base_length + 1U + strlen(relative_path) + 1U <= sizeof(candidate_path))
    {
      (void)snprintf(candidate_path, sizeof(candidate_path), "%s\\%s", current_directory, relative_path);
      if (palm_render_file_exists(candidate_path))
      {
        (void)snprintf(out_path, out_path_size, "%s", candidate_path);
        return 1;
      }
    }
  }

  if (strncmp(relative_path, k_res_prefix, strlen(k_res_prefix)) == 0)
  {
    const char* suffix = relative_path + strlen(k_res_prefix);
    for (i = 0U; i < sizeof(k_res_fallbacks) / sizeof(k_res_fallbacks[0]); ++i)
    {
      char fallback_relative[MAX_PATH] = { 0 };

      if (strlen(k_res_fallbacks[i]) + strlen(suffix) + 1U > sizeof(fallback_relative))
      {
        continue;
      }

      (void)snprintf(fallback_relative, sizeof(fallback_relative), "%s%s", k_res_fallbacks[i], suffix);
      if (palm_render_build_relative_path(module_path, fallback_relative, candidate_path, sizeof(candidate_path)) &&
        palm_render_file_exists(candidate_path))
      {
        (void)snprintf(out_path, out_path_size, "%s", candidate_path);
        return 1;
      }
    }
  }

  {
    char message[512] = { 0 };
    (void)snprintf(
      message,
      sizeof(message),
      "Failed to resolve palm asset path for '%s'. Check the res folder next to the executable or in the project root.",
      relative_path
    );
    palm_render_show_error("Path Error", message);
  }
  return 0;
}

static int palm_render_load_text_file(const char* path, const char* label, char** out_text)
{
  char message[256] = { 0 };
  FILE* file = NULL;
  long file_size = 0L;
  size_t bytes_read = 0U;
  char* text = NULL;

  #if defined(_MSC_VER)
  if (fopen_s(&file, path, "rb") != 0)
  {
    file = NULL;
  }
  #else
  file = fopen(path, "rb");
  #endif

  if (file == NULL)
  {
    (void)snprintf(message, sizeof(message), "Failed to open %s file:\n%s", label, path);
    palm_render_show_error("File Error", message);
    return 0;
  }

  if (fseek(file, 0L, SEEK_END) != 0)
  {
    (void)snprintf(message, sizeof(message), "Failed to seek %s file.", label);
    palm_render_show_error("File Error", message);
    fclose(file);
    return 0;
  }

  file_size = ftell(file);
  if (file_size < 0L || fseek(file, 0L, SEEK_SET) != 0)
  {
    (void)snprintf(message, sizeof(message), "%s file is unreadable.", label);
    palm_render_show_error("File Error", message);
    fclose(file);
    return 0;
  }

  text = (char*)malloc((size_t)file_size + 1U);
  if (text == NULL)
  {
    palm_render_show_error("Memory Error", "Failed to allocate memory for palm asset loading.");
    fclose(file);
    return 0;
  }

  bytes_read = fread(text, 1U, (size_t)file_size, file);
  fclose(file);
  if (bytes_read != (size_t)file_size)
  {
    free(text);
    (void)snprintf(message, sizeof(message), "Failed to read %s file.", label);
    palm_render_show_error("File Error", message);
    return 0;
  }

  text[file_size] = '\0';
  *out_text = text;
  return 1;
}

static char* palm_render_trim_left(char* text)
{
  while (text != NULL && (*text == ' ' || *text == '\t'))
  {
    ++text;
  }
  return text;
}

static int palm_render_reserve_memory(void** buffer, size_t* capacity, size_t required, size_t element_size)
{
  void* new_buffer = NULL;
  size_t new_capacity = 0U;

  if (buffer == NULL || capacity == NULL || element_size == 0U)
  {
    return 0;
  }
  if (required <= *capacity)
  {
    return 1;
  }

  new_capacity = (*capacity > 0U) ? *capacity : 256U;
  while (new_capacity < required)
  {
    if (new_capacity > (SIZE_MAX / 2U))
    {
      return 0;
    }
    new_capacity *= 2U;
  }
  if (new_capacity > (SIZE_MAX / element_size))
  {
    return 0;
  }

  new_buffer = realloc(*buffer, new_capacity * element_size);
  if (new_buffer == NULL)
  {
    return 0;
  }

  *buffer = new_buffer;
  *capacity = new_capacity;
  return 1;
}

static int palm_render_push_vec3(PalmVec3Array* array, PalmVec3 value)
{
  if (array == NULL || !palm_render_reserve_memory((void**)&array->data, &array->capacity, array->count + 1U, sizeof(PalmVec3)))
  {
    return 0;
  }

  array->data[array->count] = value;
  array->count += 1U;
  return 1;
}

static int palm_render_push_vertex(PalmVertexArray* array, PalmVertex value)
{
  if (array == NULL || !palm_render_reserve_memory((void**)&array->data, &array->capacity, array->count + 1U, sizeof(PalmVertex)))
  {
    return 0;
  }

  array->data[array->count] = value;
  array->count += 1U;
  return 1;
}

static PalmMaterial* palm_render_push_material(PalmMaterialArray* array, const char* name)
{
  PalmMaterial* material = NULL;

  if (array == NULL || name == NULL ||
    !palm_render_reserve_memory((void**)&array->data, &array->capacity, array->count + 1U, sizeof(PalmMaterial)))
  {
    return NULL;
  }

  material = &array->data[array->count];
  memset(material, 0, sizeof(*material));
  (void)snprintf(material->name, sizeof(material->name), "%s", name);
  material->diffuse = (PalmColor){ 0.70f, 0.70f, 0.70f };
  array->count += 1U;
  return material;
}

static int palm_render_parse_mtl(const char* mtl_path, PalmMaterialArray* materials)
{
  char* source = NULL;
  char* cursor = NULL;
  PalmMaterial* current_material = NULL;

  if (materials == NULL)
  {
    return 0;
  }
  if (!palm_render_load_text_file(mtl_path, "MTL", &source))
  {
    return 0;
  }

  cursor = source;
  while (*cursor != '\0')
  {
    char* line_start = cursor;
    char* line_end = cursor;
    char* trimmed = NULL;

    while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r')
    {
      ++line_end;
    }
    if (*line_end == '\r')
    {
      *line_end = '\0';
      cursor = line_end + 1;
      if (*cursor == '\n')
      {
        *cursor = '\0';
        cursor += 1;
      }
    }
    else if (*line_end == '\n')
    {
      *line_end = '\0';
      cursor = line_end + 1;
    }
    else
    {
      cursor = line_end;
    }

    trimmed = palm_render_trim_left(line_start);
    if (trimmed[0] == '\0' || trimmed[0] == '#')
    {
      continue;
    }

    if (strncmp(trimmed, "newmtl ", 7U) == 0)
    {
      current_material = palm_render_push_material(materials, palm_render_trim_left(trimmed + 7));
      if (current_material == NULL)
      {
        free(source);
        palm_render_show_error("Memory Error", "Failed to store palm material data.");
        return 0;
      }
    }
    else if (strncmp(trimmed, "Kd ", 3U) == 0 && current_material != NULL)
    {
      float r = 0.70f;
      float g = 0.70f;
      float b = 0.70f;

      if (palm_render_sscanf(trimmed + 3, "%f %f %f", &r, &g, &b) == 3)
      {
        current_material->diffuse = (PalmColor){ r, g, b };
      }
    }
  }

  free(source);
  return 1;
}

static const PalmMaterial* palm_render_find_material(const PalmMaterialArray* materials, const char* name)
{
  size_t i = 0U;

  if (materials == NULL || name == NULL)
  {
    return NULL;
  }

  for (i = 0U; i < materials->count; ++i)
  {
    if (strcmp(materials->data[i].name, name) == 0)
    {
      return &materials->data[i];
    }
  }

  return NULL;
}

static int palm_render_parse_face_vertex(const char* token, PalmObjIndex* out_index)
{
  const char* cursor = token;
  char* end = NULL;
  long value = 0L;

  if (token == NULL || out_index == NULL)
  {
    return 0;
  }

  memset(out_index, 0, sizeof(*out_index));
  value = strtol(cursor, &end, 10);
  if (end == cursor)
  {
    return 0;
  }
  out_index->position_index = (int)value;

  if (*end == '/')
  {
    cursor = end + 1;
    if (*cursor != '/')
    {
      value = strtol(cursor, &end, 10);
      if (end == cursor)
      {
        return 0;
      }
    }
    else
    {
      end = (char*)cursor;
    }

    if (*end == '/')
    {
      cursor = end + 1;
      value = strtol(cursor, &end, 10);
      if (end != cursor)
      {
        out_index->normal_index = (int)value;
      }
    }
  }

  return 1;
}

static int palm_render_resolve_obj_index(int obj_index, size_t count)
{
  int resolved = -1;

  if (obj_index > 0)
  {
    resolved = obj_index - 1;
  }
  else if (obj_index < 0)
  {
    resolved = (int)count + obj_index;
  }

  if (resolved < 0 || resolved >= (int)count)
  {
    return -1;
  }

  return resolved;
}

static PalmVec3 palm_render_vec3_subtract(PalmVec3 a, PalmVec3 b)
{
  PalmVec3 result = { a.x - b.x, a.y - b.y, a.z - b.z };
  return result;
}

static PalmVec3 palm_render_vec3_cross(PalmVec3 a, PalmVec3 b)
{
  PalmVec3 result = {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
  return result;
}

static float palm_render_vec3_dot(PalmVec3 a, PalmVec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static PalmVec3 palm_render_vec3_normalize(PalmVec3 value)
{
  const float length = sqrtf(palm_render_vec3_dot(value, value));
  if (length <= 0.00001f)
  {
    PalmVec3 fallback = { 0.0f, 1.0f, 0.0f };
    return fallback;
  }

  {
    const float inverse_length = 1.0f / length;
    PalmVec3 result = { value.x * inverse_length, value.y * inverse_length, value.z * inverse_length };
    return result;
  }
}

static int palm_render_load_model_vertices(HINSTANCE instance, const char* relative_obj_path, PalmVertex** out_vertices, GLsizei* out_vertex_count, float* out_model_height)
{
  char obj_path[MAX_PATH] = { 0 };
  char* source = NULL;
  char* cursor = NULL;
  PalmVec3Array positions = { 0 };
  PalmVec3Array normals = { 0 };
  PalmVertexArray vertices = { 0 };
  PalmMaterialArray materials = { 0 };
  PalmColor current_color = { 0.45f, 0.35f, 0.20f };
  PalmVec3 bounds_min = { 1.0e9f, 1.0e9f, 1.0e9f };
  PalmVec3 bounds_max = { -1.0e9f, -1.0e9f, -1.0e9f };
  PalmVec3 base_center = { 0.0f, 0.0f, 0.0f };
  size_t base_count = 0U;
  size_t i = 0U;

  if (relative_obj_path == NULL || out_vertices == NULL || out_vertex_count == NULL || out_model_height == NULL)
  {
    return 0;
  }
  *out_vertices = NULL;
  *out_vertex_count = 0;
  *out_model_height = 1.0f;

  if (!palm_render_resolve_asset_path(instance, relative_obj_path, obj_path, sizeof(obj_path)) ||
    !palm_render_load_text_file(obj_path, "OBJ", &source))
  {
    return 0;
  }

  cursor = source;
  while (*cursor != '\0')
  {
    char* line_start = cursor;
    char* line_end = cursor;
    char* trimmed = NULL;

    while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r')
    {
      ++line_end;
    }
    if (*line_end == '\r')
    {
      *line_end = '\0';
      cursor = line_end + 1;
      if (*cursor == '\n')
      {
        *cursor = '\0';
        cursor += 1;
      }
    }
    else if (*line_end == '\n')
    {
      *line_end = '\0';
      cursor = line_end + 1;
    }
    else
    {
      cursor = line_end;
    }

    trimmed = palm_render_trim_left(line_start);
    if (trimmed[0] == '\0' || trimmed[0] == '#')
    {
      continue;
    }

    if (strncmp(trimmed, "mtllib ", 7U) == 0)
    {
      char mtl_path[MAX_PATH] = { 0 };
      char relative_name[MAX_PATH] = { 0 };

      (void)snprintf(relative_name, sizeof(relative_name), "%s", palm_render_trim_left(trimmed + 7));
      if (!palm_render_build_relative_path(obj_path, relative_name, mtl_path, sizeof(mtl_path)))
      {
        diagnostics_logf("palm_render: skipped MTL reference '%s' for %s", relative_name, obj_path);
      }
      else if (!palm_render_file_exists(mtl_path))
      {
        diagnostics_logf("palm_render: missing MTL '%s' referenced by %s", mtl_path, obj_path);
      }
      else if (!palm_render_parse_mtl(mtl_path, &materials))
      {
        diagnostics_logf("palm_render: failed to parse MTL '%s', using fallback colors", mtl_path);
      }
    }
    else if (strncmp(trimmed, "usemtl ", 7U) == 0)
    {
      const PalmMaterial* material = palm_render_find_material(&materials, palm_render_trim_left(trimmed + 7));
      current_color = (material != NULL) ? material->diffuse : (PalmColor){ 0.70f, 0.70f, 0.70f };
    }
    else if (strncmp(trimmed, "v ", 2U) == 0)
    {
      PalmVec3 value = { 0 };
      if (palm_render_sscanf(trimmed + 2, "%f %f %f", &value.x, &value.y, &value.z) == 3)
      {
        if (!palm_render_push_vec3(&positions, value))
        {
          palm_render_show_error("Memory Error", "Failed to store palm OBJ positions.");
          free(source);
          free(vertices.data);
          free(positions.data);
          free(normals.data);
          free(materials.data);
          return 0;
        }
      }
    }
    else if (strncmp(trimmed, "vn ", 3U) == 0)
    {
      PalmVec3 value = { 0 };
      if (palm_render_sscanf(trimmed + 3, "%f %f %f", &value.x, &value.y, &value.z) == 3)
      {
        if (!palm_render_push_vec3(&normals, value))
        {
          palm_render_show_error("Memory Error", "Failed to store palm OBJ normals.");
          free(source);
          free(vertices.data);
          free(positions.data);
          free(normals.data);
          free(materials.data);
          return 0;
        }
      }
    }
    else if (strncmp(trimmed, "f ", 2U) == 0)
    {
      PalmObjIndex corners[PALM_RENDER_MAX_FACE_VERTICES] = { 0 };
      int corner_count = 0;
      char* token = palm_render_trim_left(trimmed + 2);

      while (token[0] != '\0' && corner_count < PALM_RENDER_MAX_FACE_VERTICES)
      {
        PalmObjIndex index = { 0 };
        char* separator = token;
        char separator_char = '\0';

        while (*separator != '\0' && *separator != ' ' && *separator != '\t')
        {
          ++separator;
        }
        separator_char = *separator;
        if (separator_char != '\0')
        {
          *separator = '\0';
        }

        if (token[0] != '\0' && palm_render_parse_face_vertex(token, &index))
        {
          corners[corner_count] = index;
          corner_count += 1;
        }

        if (separator_char == '\0')
        {
          token = separator;
        }
        else
        {
          token = palm_render_trim_left(separator + 1);
        }
      }

      if (corner_count >= 3)
      {
        int triangle_index = 0;

        for (triangle_index = 1; triangle_index + 1 < corner_count; ++triangle_index)
        {
          const PalmObjIndex triangle[3] = {
            corners[0],
            corners[triangle_index],
            corners[triangle_index + 1]
          };
          PalmVec3 triangle_positions[3];
          PalmVec3 triangle_normal = { 0.0f, 1.0f, 0.0f };
          int vertex_index = 0;

          for (vertex_index = 0; vertex_index < 3; ++vertex_index)
          {
            const int resolved_position = palm_render_resolve_obj_index(triangle[vertex_index].position_index, positions.count);
            if (resolved_position < 0)
            {
              palm_render_show_error("OBJ Error", "Palm OBJ contains an invalid position index.");
              free(source);
              free(vertices.data);
              free(positions.data);
              free(normals.data);
              free(materials.data);
              return 0;
            }

            triangle_positions[vertex_index] = positions.data[resolved_position];
          }

          triangle_normal = palm_render_vec3_normalize(
            palm_render_vec3_cross(
              palm_render_vec3_subtract(triangle_positions[1], triangle_positions[0]),
              palm_render_vec3_subtract(triangle_positions[2], triangle_positions[0])));

          for (vertex_index = 0; vertex_index < 3; ++vertex_index)
          {
            const int resolved_normal = palm_render_resolve_obj_index(triangle[vertex_index].normal_index, normals.count);
            const PalmVec3 normal = (resolved_normal >= 0) ? normals.data[resolved_normal] : triangle_normal;
            PalmVertex vertex = {
              { triangle_positions[vertex_index].x, triangle_positions[vertex_index].y, triangle_positions[vertex_index].z },
              { normal.x, normal.y, normal.z },
              { current_color.r, current_color.g, current_color.b }
            };

            if (!palm_render_push_vertex(&vertices, vertex))
            {
              palm_render_show_error("Memory Error", "Failed to store palm OBJ triangles.");
              free(source);
              free(vertices.data);
              free(positions.data);
              free(normals.data);
              free(materials.data);
              return 0;
            }
          }
        }
      }
    }
  }

  if (positions.count == 0U || vertices.count == 0U)
  {
    palm_render_show_error("OBJ Error", "Palm OBJ did not contain any renderable geometry.");
    free(source);
    free(vertices.data);
    free(positions.data);
    free(normals.data);
    free(materials.data);
    return 0;
  }

  for (i = 0U; i < positions.count; ++i)
  {
    const PalmVec3 value = positions.data[i];
    if (value.x < bounds_min.x)
    {
      bounds_min.x = value.x;
    }
    if (value.y < bounds_min.y)
    {
      bounds_min.y = value.y;
    }
    if (value.z < bounds_min.z)
    {
      bounds_min.z = value.z;
    }
    if (value.x > bounds_max.x)
    {
      bounds_max.x = value.x;
    }
    if (value.y > bounds_max.y)
    {
      bounds_max.y = value.y;
    }
    if (value.z > bounds_max.z)
    {
      bounds_max.z = value.z;
    }
  }

  for (i = 0U; i < positions.count; ++i)
  {
    const PalmVec3 value = positions.data[i];
    if (value.y <= bounds_min.y + 5.0f)
    {
      base_center.x += value.x;
      base_center.z += value.z;
      base_count += 1U;
    }
  }
  if (base_count > 0U)
  {
    base_center.x /= (float)base_count;
    base_center.z /= (float)base_count;
  }

  for (i = 0U; i < vertices.count; ++i)
  {
    vertices.data[i].position[0] -= base_center.x;
    vertices.data[i].position[1] -= bounds_min.y;
    vertices.data[i].position[2] -= base_center.z;
  }

  *out_vertices = vertices.data;
  *out_vertex_count = (GLsizei)vertices.count;
  *out_model_height = palm_render_clamp(bounds_max.y - bounds_min.y, 1.0f, 10000.0f);

  diagnostics_logf(
    "palm_render: loaded OBJ vertices=%d height=%.2f source=%s",
    (int)*out_vertex_count,
    *out_model_height,
    obj_path);

  free(source);
  free(positions.data);
  free(normals.data);
  free(materials.data);
  return 1;
}

static int palm_render_reserve_instances(PalmRenderVariant* variant, size_t required_instance_capacity)
{
  if (variant == NULL)
  {
    return 0;
  }

  if (!palm_render_reserve_memory(
    &variant->cpu_instances,
    &variant->cpu_instance_capacity,
    required_instance_capacity,
    sizeof(PalmInstanceData)))
  {
    palm_render_show_error("Memory Error", "Failed to allocate palm instance data.");
    return 0;
  }

  return 1;
}

static GLsizei palm_render_get_max_vertex_count(const PalmRenderMesh* mesh)
{
  GLsizei max_vertex_count = 0;
  int variant_index = 0;

  if (mesh == NULL)
  {
    return 0;
  }

  for (variant_index = 0; variant_index < mesh->variant_count; ++variant_index)
  {
    if (mesh->variants[variant_index].vertex_count > max_vertex_count)
    {
      max_vertex_count = mesh->variants[variant_index].vertex_count;
    }
  }

  return max_vertex_count;
}

static float palm_render_clamp(float value, float min_value, float max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return value;
}

static float palm_render_mix(float a, float b, float t)
{
  return a + (b - a) * t;
}

static float palm_render_hash_unit(int x, int z, unsigned int seed)
{
  unsigned int state = (unsigned int)(x * 374761393) ^ (unsigned int)(z * 668265263) ^ (seed * 2246822519U);
  state = (state ^ (state >> 13)) * 1274126177U;
  state ^= state >> 16;
  return (float)(state & 0x00FFFFFFU) / (float)0x01000000U;
}

static float palm_render_estimate_slope(float x, float z, const SceneSettings* settings)
{
  const float sample_offset = 3.0f;
  const float x0 = terrain_get_height(x - sample_offset, z, settings);
  const float x1 = terrain_get_height(x + sample_offset, z, settings);
  const float z0 = terrain_get_height(x, z - sample_offset, settings);
  const float z1 = terrain_get_height(x, z + sample_offset, settings);
  const float dx = fabsf(x1 - x0) / (sample_offset * 2.0f);
  const float dz = fabsf(z1 - z0) / (sample_offset * 2.0f);

  return dx + dz;
}

static void palm_render_build_instance_transform(PalmInstanceData* instance, float x, float y, float z, float scale, float yaw_radians, PalmColor tint)
{
  const float c = cosf(yaw_radians) * scale;
  const float s = sinf(yaw_radians) * scale;

  if (instance == NULL)
  {
    return;
  }

  memset(instance, 0, sizeof(*instance));
  instance->transform[0] = c;
  instance->transform[2] = s;
  instance->transform[5] = scale;
  instance->transform[8] = -s;
  instance->transform[10] = c;
  instance->transform[12] = x;
  instance->transform[13] = y;
  instance->transform[14] = z;
  instance->transform[15] = 1.0f;
  instance->tint[0] = palm_render_clamp(tint.r, 0.75f, 1.25f);
  instance->tint[1] = palm_render_clamp(tint.g, 0.75f, 1.25f);
  instance->tint[2] = palm_render_clamp(tint.b, 0.75f, 1.25f);
  instance->tint[3] = 1.0f;
}
