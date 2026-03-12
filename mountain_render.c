#include "mountain_render.h"

int mountain_render_create(MountainRenderMesh* mesh)
{
  return palm_render_create_category(mesh, PALM_RENDER_CATEGORY_MOUNTAIN);
}

void mountain_render_destroy(MountainRenderMesh* mesh)
{
  palm_render_destroy(mesh);
}

int mountain_render_update(
  MountainRenderMesh* mesh,
  const CameraState* camera,
  const SceneSettings* settings,
  const RendererQualityProfile* quality,
  const ViewFrustum* frustum)
{
  return palm_render_update_category_with_frustum(mesh, PALM_RENDER_CATEGORY_MOUNTAIN, camera, settings, quality, frustum);
}

void mountain_render_draw(const MountainRenderMesh* mesh)
{
  palm_render_draw(mesh);
}
