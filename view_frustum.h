#ifndef VIEW_FRUSTUM_H
#define VIEW_FRUSTUM_H

#include "camera.h"
#include "math3d.h"

typedef struct ViewFrustum
{
  Matrix view_matrix;
  float tan_half_fov_y;
  float aspect;
  float near_plane;
  float far_plane;
  int valid;
} ViewFrustum;

ViewFrustum view_frustum_build(
  const CameraState* camera,
  int width,
  int height,
  float fov_degrees);
int view_frustum_contains_sphere(
  const ViewFrustum* frustum,
  float x,
  float y,
  float z,
  float radius);

#endif
