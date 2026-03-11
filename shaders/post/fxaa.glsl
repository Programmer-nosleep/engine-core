float post_luminance(vec3 rgb)
{
  return dot(rgb, vec3(0.299, 0.587, 0.114));
}

vec3 post_sample_scene_color(vec2 uv)
{
  return texture(tex[0], clamp(uv, vec2(0.0), vec2(1.0))).rgb;
}

float post_sample_depth(vec2 uv)
{
  return texture(tex[1], clamp(uv, vec2(0.0), vec2(1.0))).r;
}

float post_depth_edge_strength(vec2 uv, vec2 texel_size)
{
  float center_depth = post_sample_depth(uv);
  float north_depth = post_sample_depth(uv + vec2(0.0, texel_size.y));
  float south_depth = post_sample_depth(uv - vec2(0.0, texel_size.y));
  float east_depth = post_sample_depth(uv + vec2(texel_size.x, 0.0));
  float west_depth = post_sample_depth(uv - vec2(texel_size.x, 0.0));
  float depth_delta = max(
    max(abs(center_depth - north_depth), abs(center_depth - south_depth)),
    max(abs(center_depth - east_depth), abs(center_depth - west_depth))
  );

  return smoothstep(0.0004, 0.0060, depth_delta);
}

vec3 post_apply_detail_recovery(vec2 uv, vec2 texel_size, vec3 filtered_color, float render_scale, float edge_strength)
{
  vec3 center = post_sample_scene_color(uv);
  vec3 neighborhood = vec3(0.0);
  float upscale_bias = clamp((1.0 / max(render_scale, 0.65)) - 1.0, 0.0, 0.45);

  if (upscale_bias <= 0.001)
  {
    return filtered_color;
  }

  neighborhood += post_sample_scene_color(uv + vec2(texel_size.x, 0.0));
  neighborhood += post_sample_scene_color(uv - vec2(texel_size.x, 0.0));
  neighborhood += post_sample_scene_color(uv + vec2(0.0, texel_size.y));
  neighborhood += post_sample_scene_color(uv - vec2(0.0, texel_size.y));
  neighborhood *= 0.25;

  {
    vec3 sharpened = filtered_color + (center - neighborhood) * (0.18 * upscale_bias);
    float sharpen_mix = (1.0 - edge_strength) * smoothstep(0.03, 0.30, upscale_bias);
    return mix(filtered_color, clamp(sharpened, 0.0, 1.0), sharpen_mix);
  }
}

vec3 post_apply_fxaa(vec2 uv, vec2 texel_size, float render_scale)
{
  vec3 rgb_m = post_sample_scene_color(uv);
  vec3 rgb_nw = post_sample_scene_color(uv + texel_size * vec2(-1.0, -1.0));
  vec3 rgb_ne = post_sample_scene_color(uv + texel_size * vec2(1.0, -1.0));
  vec3 rgb_sw = post_sample_scene_color(uv + texel_size * vec2(-1.0, 1.0));
  vec3 rgb_se = post_sample_scene_color(uv + texel_size * vec2(1.0, 1.0));
  float luma_m = post_luminance(rgb_m);
  float luma_nw = post_luminance(rgb_nw);
  float luma_ne = post_luminance(rgb_ne);
  float luma_sw = post_luminance(rgb_sw);
  float luma_se = post_luminance(rgb_se);
  float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
  float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));
  float luma_range = luma_max - luma_min;
  float depth_edge = post_depth_edge_strength(uv, texel_size);
  float local_contrast_threshold = mix(0.028, 0.020, clamp(1.0 - render_scale, 0.0, 1.0));
  float relative_threshold = max(local_contrast_threshold, luma_max * 0.12);
  vec2 direction = vec2(
    -((luma_nw + luma_ne) - (luma_sw + luma_se)),
    ((luma_nw + luma_sw) - (luma_ne + luma_se))
  );
  float direction_reduce = max(
    (luma_nw + luma_ne + luma_sw + luma_se) * (0.25 * (1.0 / 8.0)),
    1.0 / 128.0
  );
  float reciprocal_direction_min = 1.0 / (min(abs(direction.x), abs(direction.y)) + direction_reduce);
  float edge_strength = max(smoothstep(relative_threshold, relative_threshold * 3.0, luma_range), depth_edge * 0.58);

  if (edge_strength <= 0.001)
  {
    return post_apply_detail_recovery(uv, texel_size, rgb_m, render_scale, 0.0);
  }

  direction = clamp(
    direction * reciprocal_direction_min,
    vec2(-8.0 - depth_edge * 4.0),
    vec2(8.0 + depth_edge * 4.0)
  ) * texel_size;

  {
    vec3 rgb_a = 0.5 * (
      post_sample_scene_color(uv + direction * (1.0 / 3.0 - 0.5)) +
      post_sample_scene_color(uv + direction * (2.0 / 3.0 - 0.5))
    );
    vec3 rgb_b = rgb_a * 0.5 + 0.25 * (
      post_sample_scene_color(uv + direction * -0.5) +
      post_sample_scene_color(uv + direction * 0.5)
    );
    float luma_b = post_luminance(rgb_b);
    vec3 filtered_color = (luma_b < luma_min || luma_b > luma_max) ? rgb_a : rgb_b;

    filtered_color = mix(rgb_m, filtered_color, clamp(edge_strength * 0.88, 0.0, 1.0));
    return post_apply_detail_recovery(uv, texel_size, filtered_color, render_scale, edge_strength);
  }
}
