#ifndef MATH3D_H
#define MATH3D_H

typedef struct Matrix
{
  float m[16];
} Matrix;

Matrix math_get_projection_matrix(int width, int height, float fov_degrees);
Matrix math_get_view_matrix(float x, float y, float z, float yaw, float pitch);
Matrix math_matrix_multiply(const Matrix* a, const Matrix* b);
Matrix math_get_orthographic_matrix(float left, float right, float bottom, float top, float near_plane, float far_plane);
Matrix math_get_look_at_matrix(
  float eye_x,
  float eye_y,
  float eye_z,
  float target_x,
  float target_y,
  float target_z,
  float up_x,
  float up_y,
  float up_z
);

#endif
