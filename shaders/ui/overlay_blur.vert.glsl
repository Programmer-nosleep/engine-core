#version 330 core

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;

uniform vec2 viewport_size;

out vec2 v_uv;

void main()
{
  vec2 ndc = vec2(
    (in_position.x / viewport_size.x) * 2.0 - 1.0,
    1.0 - (in_position.y / viewport_size.y) * 2.0
  );

  v_uv = in_uv;
  gl_Position = vec4(ndc, 0.0, 1.0);
}
