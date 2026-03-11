#ifndef PALM_RENDER_H
#define PALM_RENDER_H

#include "camera.h"
#include "gl_headers.h"
#include "render_quality.h"
#include "scene_settings.h"

#include <stddef.h>

enum
{
  PALM_RENDER_MAX_VARIANTS = 3
};

typedef struct PalmRenderVariant
{
  GLuint vao;
  GLuint vertex_buffer;
  GLuint instance_buffer;
  GLsizei vertex_count;
  GLsizei instance_count;
  void* cpu_instances;
  size_t cpu_instance_capacity;
  float model_height;
} PalmRenderVariant;

typedef struct PalmRenderMesh
{
  PalmRenderVariant variants[PALM_RENDER_MAX_VARIANTS];
  int variant_count;
} PalmRenderMesh;

int palm_render_create(PalmRenderMesh* mesh, HINSTANCE instance);
void palm_render_destroy(PalmRenderMesh* mesh);
int palm_render_update(
  PalmRenderMesh* mesh,
  const CameraState* camera,
  const SceneSettings* settings,
  const RendererQualityProfile* quality);
void palm_render_draw(const PalmRenderMesh* mesh);

#endif
