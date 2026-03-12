#include "view_frustum.h"

#include <math.h>

ViewFrustum view_frustum_build(
  const CameraState* camera,
  int width,
  int height,
  float fov_degrees)
{
  const float pi = 3.14159265f;
  const float safe_fov = (fov_degrees > 1.0f) ? fov_degrees : 65.0f;
  const int safe_height = (height > 0) ? height : 1;
  const float aspect = (width > 0) ? ((float)width / (float)safe_height) : 1.0f;
  ViewFrustum frustum;

  frustum.view_matrix = math_get_view_matrix(
    (camera != NULL) ? camera->x : 0.0f,
    (camera != NULL) ? camera->y : 0.0f,
    (camera != NULL) ? camera->z : 0.0f,
    (camera != NULL) ? camera->yaw : 0.0f,
    (camera != NULL) ? camera->pitch : 0.0f);
  frustum.tan_half_fov_y = tanf(safe_fov * pi / 180.0f * 0.5f);
  frustum.aspect = aspect;
  frustum.near_plane = 1.0f;
  frustum.far_plane = 5000.0f;
  frustum.valid = (camera != NULL && width > 0 && height > 0) ? 1 : 0;
  return frustum;
}

int view_frustum_contains_sphere(
  const ViewFrustum* frustum,
  float x,
  float y,
  float z,
  float radius)
{
  float safe_radius = radius;
  float view_x = 0.0f;
  float view_y = 0.0f;
  float view_z = 0.0f;
  float depth = 0.0f;
  float half_height = 0.0f;
  float half_width = 0.0f;

  if (frustum == NULL || frustum->valid == 0)
  {
    return 1;
  }

  if (safe_radius < 0.0f)
  {
    safe_radius = 0.0f;
  }

  view_x =
    frustum->view_matrix.m[0] * x +
    frustum->view_matrix.m[4] * y +
    frustum->view_matrix.m[8] * z +
    frustum->view_matrix.m[12];
  view_y =
    frustum->view_matrix.m[1] * x +
    frustum->view_matrix.m[5] * y +
    frustum->view_matrix.m[9] * z +
    frustum->view_matrix.m[13];
  view_z =
    frustum->view_matrix.m[2] * x +
    frustum->view_matrix.m[6] * y +
    frustum->view_matrix.m[10] * z +
    frustum->view_matrix.m[14];
  depth = -view_z;

  if (depth + safe_radius < frustum->near_plane)
  {
    return 0;
  }
  if (depth - safe_radius > frustum->far_plane)
  {
    return 0;
  }
  if (depth < 0.0f)
  {
    depth = 0.0f;
  }

  half_height = depth * frustum->tan_half_fov_y;
  half_width = half_height * frustum->aspect;

  if (view_x < -half_width - safe_radius || view_x > half_width + safe_radius)
  {
    return 0;
  }
  if (view_y < -half_height - safe_radius || view_y > half_height + safe_radius)
  {
    return 0;
  }

  return 1;
}
