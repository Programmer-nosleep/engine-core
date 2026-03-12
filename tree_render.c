#include "tree_render.h"

int tree_render_create(TreeRenderMesh* mesh)
{
  return palm_render_create_category(mesh, PALM_RENDER_CATEGORY_TREE);
}

void tree_render_destroy(TreeRenderMesh* mesh)
{
  palm_render_destroy(mesh);
}

int tree_render_update(
  TreeRenderMesh* mesh,
  const CameraState* camera,
  const SceneSettings* settings,
  const RendererQualityProfile* quality,
  const ViewFrustum* frustum)
{
  return palm_render_update_category_with_frustum(mesh, PALM_RENDER_CATEGORY_TREE, camera, settings, quality, frustum);
}

void tree_render_draw(const TreeRenderMesh* mesh)
{
  palm_render_draw(mesh);
}
