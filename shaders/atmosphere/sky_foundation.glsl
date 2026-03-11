vec3 sky_foundation_atmosphere(vec3 direction, vec3 sun_dir, float daylight)
{
  const float Br = 0.0025;
  const float Bm = 0.0003;
  const float g = 0.9800;
  const vec3 nitrogen = vec3(0.650, 0.570, 0.475);
  const vec3 Kr = Br / pow(nitrogen, vec3(4.0));
  const vec3 Km = Bm / pow(nitrogen, vec3(0.84));

  vec3 view_dir = normalize(direction);
  vec3 light_dir = normalize(sun_dir);
  float mu = dot(view_dir, light_dir);
  float up = clamp(view_dir.y, 0.0, 1.0);
  float rayleigh = 3.0 / (8.0 * 3.14159265) * (1.0 + mu * mu);
  vec3 mie = (Kr + Km * (1.0 - g * g) / (2.0 + g * g) / pow(max(1.0 + g * g - 2.0 * g * mu, 0.001), 1.5)) / (Br + Bm);
  vec3 day_extinction =
    exp(-exp(-((up + light_dir.y * 4.0) * (exp(-up * 16.0) + 0.1) / 80.0) / Br) * (exp(-up * 16.0) + 0.1) * Kr / Br) *
    exp(-up * exp(-up * 8.0) * 4.0) *
    exp(-up * 2.0) * 4.0;
  vec3 night_extinction = vec3(1.0 - exp(light_dir.y)) * 0.2;
  vec3 extinction = mix(day_extinction, night_extinction, clamp(-light_dir.y * 0.2 + 0.5, 0.0, 1.0));
  vec3 scatter = rayleigh * mie * extinction;
  vec3 day_horizon = vec3(0.54, 0.68, 0.86);
  vec3 day_zenith = vec3(0.16, 0.34, 0.68);
  vec3 night_horizon = vec3(0.022, 0.032, 0.060);
  vec3 night_zenith = vec3(0.005, 0.010, 0.024);
  vec3 horizon_color = mix(night_horizon, day_horizon, daylight);
  vec3 zenith_color = mix(night_zenith, day_zenith, daylight);
  vec3 gradient = mix(horizon_color, zenith_color, pow(up, 0.58));
  float horizon_boost = 0.36 + 0.34 * pow(1.0 - up, 0.55);
  float forward_dampen = mix(0.78, 1.0, pow(up, 0.65));

  return scatter * forward_dampen + gradient * mix(0.14, 0.28, daylight) * horizon_boost;
}

float sky_foundation_sun_visibility(vec3 sun_dir)
{
  return smoothstep(0.04, 0.24, sun_dir.y);
}

float sky_foundation_sun_disk(vec3 direction, vec3 sun_dir, float distance_factor)
{
  float sun_amount = max(dot(normalize(direction), normalize(sun_dir)), 0.0);
  return smoothstep(
    mix(0.99992, 0.99984, distance_factor),
    mix(0.999998, 0.999991, distance_factor),
    sun_amount);
}

float sky_foundation_sun_halo(vec3 direction, vec3 sun_dir, float distance_factor)
{
  float sun_amount = max(dot(normalize(direction), normalize(sun_dir)), 0.0);
  return pow(sun_amount, mix(144.0, 108.0, distance_factor));
}
