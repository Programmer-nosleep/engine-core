vec3 ui_blur_kawase(sampler2D source_texture, vec2 uv, vec2 texel_size, float radius)
{
  vec2 inner_step = texel_size * max(radius, 0.15) * 11.0;
  vec2 outer_step = inner_step * 1.9;
  vec2 far_step = inner_step * 2.8;
  vec3 blurred = texture(source_texture, clamp(uv, vec2(0.0), vec2(1.0))).rgb * 1.5;
  float total_weight = 1.5;

  blurred += texture(source_texture, clamp(uv + vec2(-inner_step.x, -inner_step.y), vec2(0.0), vec2(1.0))).rgb * 1.1;
  blurred += texture(source_texture, clamp(uv + vec2(inner_step.x, -inner_step.y), vec2(0.0), vec2(1.0))).rgb * 1.1;
  blurred += texture(source_texture, clamp(uv + vec2(-inner_step.x, inner_step.y), vec2(0.0), vec2(1.0))).rgb * 1.1;
  blurred += texture(source_texture, clamp(uv + vec2(inner_step.x, inner_step.y), vec2(0.0), vec2(1.0))).rgb * 1.1;
  total_weight += 4.4;

  blurred += texture(source_texture, clamp(uv + vec2(-outer_step.x, 0.0), vec2(0.0), vec2(1.0))).rgb * 0.85;
  blurred += texture(source_texture, clamp(uv + vec2(outer_step.x, 0.0), vec2(0.0), vec2(1.0))).rgb * 0.85;
  blurred += texture(source_texture, clamp(uv + vec2(0.0, -outer_step.y), vec2(0.0), vec2(1.0))).rgb * 0.85;
  blurred += texture(source_texture, clamp(uv + vec2(0.0, outer_step.y), vec2(0.0), vec2(1.0))).rgb * 0.85;
  total_weight += 3.4;

  blurred += texture(source_texture, clamp(uv + vec2(-far_step.x, -far_step.y), vec2(0.0), vec2(1.0))).rgb * 0.55;
  blurred += texture(source_texture, clamp(uv + vec2(far_step.x, -far_step.y), vec2(0.0), vec2(1.0))).rgb * 0.55;
  blurred += texture(source_texture, clamp(uv + vec2(-far_step.x, far_step.y), vec2(0.0), vec2(1.0))).rgb * 0.55;
  blurred += texture(source_texture, clamp(uv + vec2(far_step.x, far_step.y), vec2(0.0), vec2(1.0))).rgb * 0.55;
  total_weight += 2.2;

  return blurred / max(total_weight, 0.0001);
}
