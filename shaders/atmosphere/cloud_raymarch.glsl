float cloud_sample_density(vec2 uv, float layer, float cloud_amount, float cloud_spacing, float cloud_time_value)
{
  vec2 primary_wind = vec2(cloud_time_value * 0.065, cloud_time_value * 0.018);
  vec2 detail_wind = vec2(-cloud_time_value * 0.032, cloud_time_value * 0.048);
  float spacing_scale = mix(2.8, 0.65, clamp((cloud_spacing - 0.45) / 1.75, 0.0, 1.0));
  vec3 field = vec3((uv + primary_wind) * spacing_scale, layer * 3.6 + cloud_time_value * 0.09);
  float broad = fbm(field);
  float detail = noise(vec3((uv + detail_wind) * spacing_scale * 2.7 + vec2(1.7, -0.9), layer * 8.4 + cloud_time_value * 0.28));
  float shape = mix(broad, detail, 0.26);
  float coverage = mix(0.76, 0.38, clamp(cloud_amount, 0.0, 1.0));
  return smoothstep(coverage, 0.94, shape);
}

vec4 raymarch_cloud_layer(
  vec3 view_dir,
  vec3 light_dir,
  vec3 sun_color,
  float daylight,
  float cloud_time_value,
  float cloud_amount,
  float cloud_spacing,
  float quality_mode
)
{
  const int max_steps = 6;
  int step_count = (quality_mode > 0.5) ? 6 : 3;
  float transmittance = 1.0;
  float horizon_fade = smoothstep(0.03, 0.18, view_dir.y);
  vec3 accumulation = vec3(0.0);

  if (view_dir.y <= 0.02)
  {
    return vec4(0.0);
  }

  for (int i = 0; i < max_steps; ++i)
  {
    float layer = 0.0;
    float march_depth = 0.0;
    vec3 sample_pos = vec3(0.0);
    vec2 uv = vec2(0.0);
    float density = 0.0;
    float silver = 0.0;
    vec3 cloud_color = vec3(0.0);

    if (i >= step_count)
    {
      break;
    }

    layer = mix(0.18, 0.44, float(i) / float(max_steps - 1));
    march_depth = (0.34 + layer) / max(view_dir.y + layer * 0.14, 0.08);
    sample_pos = view_dir * march_depth;
    uv = sample_pos.xz / max(sample_pos.y + 0.22, 0.10);
    uv += vec2(cloud_time_value * (0.050 + layer * 0.035), cloud_time_value * (0.014 + layer * 0.010));
    density = cloud_sample_density(uv, layer, cloud_amount, cloud_spacing, cloud_time_value);
    density *= horizon_fade * (0.26 + cloud_amount * 0.74);

    if (density <= 0.001)
    {
      continue;
    }

    silver = pow(max(dot(normalize(vec3(uv.x * 0.16, 0.35 + layer * 0.4, uv.y * 0.16)), light_dir), 0.0), 14.0);
    cloud_color = mix(vec3(0.42, 0.46, 0.52), vec3(0.92, 0.96, 0.99), daylight);
    cloud_color = mix(cloud_color, sun_color * 0.72 + vec3(0.06), silver * 0.12);
    accumulation += cloud_color * density * transmittance * 0.55;
    transmittance *= 1.0 - density * 0.62;

    if (transmittance < 0.04)
    {
      break;
    }
  }

  return vec4(accumulation, 1.0 - transmittance);
}
