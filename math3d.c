#include "math3d.h"

#include <math.h>

typedef struct MathVec3
{
  float x;
  float y;
  float z;
} MathVec3;

static MathVec3 math_vec3_subtract(MathVec3 a, MathVec3 b);
static MathVec3 math_vec3_cross(MathVec3 a, MathVec3 b);
static float math_vec3_dot(MathVec3 a, MathVec3 b);
static MathVec3 math_vec3_normalize(MathVec3 value);

Matrix math_get_projection_matrix(int width, int height, float fov_degrees)
{
  const float pi = 3.14159265f;
  const float fov = (fov_degrees > 1.0f) ? fov_degrees : 65.0f;
  const float near_plane = 1.0f;
  const float far_plane = 5000.0f;
  const int safe_height = (height > 0) ? height : 1;
  const float aspect = (float)width / (float)safe_height;
  const float tan_half_fov = tanf(fov * pi / 180.0f / 2.0f);

  return (Matrix){ .m = {
    [0] = 1.0f / (aspect * tan_half_fov),
    [5] = 1.0f / tan_half_fov,
    [10] = -(far_plane + near_plane) / (far_plane - near_plane),
    [11] = -1.0f,
    [14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane)
  } };
}

Matrix math_get_view_matrix(float x, float y, float z, float yaw, float pitch)
{
  const float cosy = cosf(yaw);
  const float siny = sinf(yaw);
  const float cosp = cosf(pitch);
  const float sinp = sinf(pitch);

  return (Matrix){ .m = {
    [0] = cosy,
    [1] = siny * sinp,
    [2] = siny * cosp,
    [5] = cosp,
    [6] = -sinp,
    [8] = -siny,
    [9] = cosy * sinp,
    [10] = cosp * cosy,
    [12] = -(cosy * x - siny * z),
    [13] = -(siny * sinp * x + cosp * y + cosy * sinp * z),
    [14] = -(siny * cosp * x - sinp * y + cosp * cosy * z),
    [15] = 1.0f
  } };
}

Matrix math_matrix_multiply(const Matrix* a, const Matrix* b)
{
  Matrix result = { 0 };
  int column = 0;
  int row = 0;

  for (column = 0; column < 4; ++column)
  {
    for (row = 0; row < 4; ++row)
    {
      result.m[column * 4 + row] =
        a->m[0 * 4 + row] * b->m[column * 4 + 0] +
        a->m[1 * 4 + row] * b->m[column * 4 + 1] +
        a->m[2 * 4 + row] * b->m[column * 4 + 2] +
        a->m[3 * 4 + row] * b->m[column * 4 + 3];
    }
  }

  return result;
}

Matrix math_get_orthographic_matrix(float left, float right, float bottom, float top, float near_plane, float far_plane)
{
  return (Matrix){ .m = {
    [0] = 2.0f / (right - left),
    [5] = 2.0f / (top - bottom),
    [10] = -2.0f / (far_plane - near_plane),
    [12] = -(right + left) / (right - left),
    [13] = -(top + bottom) / (top - bottom),
    [14] = -(far_plane + near_plane) / (far_plane - near_plane),
    [15] = 1.0f
  } };
}

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
)
{
  const MathVec3 eye = { eye_x, eye_y, eye_z };
  const MathVec3 target = { target_x, target_y, target_z };
  const MathVec3 up = { up_x, up_y, up_z };
  const MathVec3 forward = math_vec3_normalize(math_vec3_subtract(target, eye));
  const MathVec3 side = math_vec3_normalize(math_vec3_cross(forward, up));
  const MathVec3 real_up = math_vec3_cross(side, forward);

  return (Matrix){ .m = {
    [0] = side.x,
    [1] = real_up.x,
    [2] = -forward.x,
    [4] = side.y,
    [5] = real_up.y,
    [6] = -forward.y,
    [8] = side.z,
    [9] = real_up.z,
    [10] = -forward.z,
    [12] = -math_vec3_dot(side, eye),
    [13] = -math_vec3_dot(real_up, eye),
    [14] = math_vec3_dot(forward, eye),
    [15] = 1.0f
  } };
}

static MathVec3 math_vec3_subtract(MathVec3 a, MathVec3 b)
{
  MathVec3 result = { a.x - b.x, a.y - b.y, a.z - b.z };
  return result;
}

static MathVec3 math_vec3_cross(MathVec3 a, MathVec3 b)
{
  MathVec3 result = {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
  return result;
}

static float math_vec3_dot(MathVec3 a, MathVec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static MathVec3 math_vec3_normalize(MathVec3 value)
{
  const float length = sqrtf(math_vec3_dot(value, value));
  if (length <= 0.00001f)
  {
    MathVec3 fallback = { 0.0f, 1.0f, 0.0f };
    return fallback;
  }

  {
    const float inverse_length = 1.0f / length;
    MathVec3 result = { value.x * inverse_length, value.y * inverse_length, value.z * inverse_length };
    return result;
  }
}
