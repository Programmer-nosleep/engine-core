const COLOR_STOPS = [
  { stop: 0.0, color: [18, 46, 68] },
  { stop: 0.16, color: [62, 99, 116] },
  { stop: 0.34, color: [132, 125, 92] },
  { stop: 0.56, color: [103, 142, 80] },
  { stop: 0.78, color: [109, 95, 73] },
  { stop: 1.0, color: [233, 224, 210] },
];

function clamp(value, minimum, maximum) {
  return Math.min(Math.max(value, minimum), maximum);
}

function clamp01(value) {
  return clamp(value, 0, 1);
}

function normalizeVector(x, y, z) {
  const length = Math.hypot(x, y, z) || 1;
  return [x / length, y / length, z / length];
}

function sampleColorRamp(t) {
  const clamped = clamp01(t);

  for (let index = 1; index < COLOR_STOPS.length; index += 1) {
    const previous = COLOR_STOPS[index - 1];
    const current = COLOR_STOPS[index];
    if (clamped <= current.stop) {
      const localT = (clamped - previous.stop) / (current.stop - previous.stop || 1);
      return [
        previous.color[0] + (current.color[0] - previous.color[0]) * localT,
        previous.color[1] + (current.color[1] - previous.color[1]) * localT,
        previous.color[2] + (current.color[2] - previous.color[2]) * localT,
      ];
    }
  }

  return [...COLOR_STOPS[COLOR_STOPS.length - 1].color];
}

function getLightDirection(settings, timeOfDay) {
  const orbitDegrees = settings?.sunOrbit ?? 82.8;
  const daylightFraction = clamp(settings?.daylightFraction ?? 0.5, 0.3, 0.75);
  const sunrise = 0.5 - daylightFraction * 0.5;
  const sunset = 0.5 + daylightFraction * 0.5;
  const nightDuration = Math.max((1 + sunrise) - sunset, 0.001);
  let wrappedTime = timeOfDay ?? 0.23;
  let orbitAngle = 0;

  if (wrappedTime < sunrise) {
    wrappedTime += 1;
  }

  if (wrappedTime < sunset) {
    orbitAngle = ((wrappedTime - sunrise) / Math.max(sunset - sunrise, 0.001)) * Math.PI;
  } else {
    orbitAngle = Math.PI + ((wrappedTime - sunset) / nightDuration) * Math.PI;
  }

  const azimuth = ((orbitDegrees - 90) * Math.PI) / 180;
  return normalizeVector(
    Math.cos(orbitAngle) * Math.sin(azimuth),
    Math.sin(orbitAngle),
    Math.cos(orbitAngle) * Math.cos(azimuth),
  );
}

function computeShade(heights, width, height, x, y, lightDirection) {
  const left = heights[y * width + Math.max(x - 1, 0)];
  const right = heights[y * width + Math.min(x + 1, width - 1)];
  const up = heights[Math.max(y - 1, 0) * width + x];
  const down = heights[Math.min(y + 1, height - 1) * width + x];
  const normal = normalizeVector(left - right, 2.1, up - down);
  const light = normal[0] * lightDirection[0] + normal[1] * lightDirection[1] + normal[2] * lightDirection[2];

  return 0.56 + light * 0.44;
}

function worldToCanvas(x, z, view) {
  return {
    pixelX: ((x - view.centerX) / view.step) + (view.sampleWidth - 1) * 0.5,
    pixelY: ((z - view.centerZ) / view.step) + (view.sampleHeight - 1) * 0.5,
  };
}

function drawCloudLayer(context, width, height, settings, runtime) {
  const cloudsEnabled = settings?.cloudsEnabled ?? true;
  const cloudAmount = clamp(settings?.cloudAmount ?? 0.54, 0, 1);

  if (!cloudsEnabled || cloudAmount <= 0.02) {
    return;
  }

  const cloudSpacing = clamp(settings?.cloudSpacing ?? 1, 0.45, 2.2);
  const drift = (runtime?.elapsedSeconds ?? 0) * (16 / cloudSpacing);
  const count = Math.max(5, Math.round(6 + cloudAmount * 10));

  context.save();
  context.globalCompositeOperation = "screen";
  context.globalAlpha = 0.05 + cloudAmount * 0.12;

  for (let index = 0; index < count; index += 1) {
    const seed = index * 137.47;
    const centerX = ((seed * 1.9 + drift * 8) % (width + 280)) - 140;
    const centerY =
      height * (0.14 + (index % 5) * 0.085) +
      Math.sin(drift * 0.02 + seed * 0.01) * 18;
    const radiusX = 82 + (index % 4) * 26 + cloudAmount * 54;
    const radiusY = radiusX * (0.2 + (2.2 - cloudSpacing) * 0.04);
    const gradient = context.createRadialGradient(
      centerX - radiusX * 0.18,
      centerY - radiusY * 0.12,
      radiusX * 0.18,
      centerX,
      centerY,
      radiusX,
    );

    gradient.addColorStop(0, "rgba(255,255,255,0.95)");
    gradient.addColorStop(0.62, "rgba(244,249,252,0.28)");
    gradient.addColorStop(1, "rgba(244,249,252,0)");

    context.fillStyle = gradient;
    context.beginPath();
    context.ellipse(centerX, centerY, radiusX, radiusY, -0.08, 0, Math.PI * 2);
    context.fill();
  }

  context.restore();
}

function drawAtmosphereOverlay(context, width, height, settings, lightDirection) {
  const fogDensity = clamp(settings?.fogDensity ?? 0.24, 0, 1);
  const daylight = clamp01(lightDirection[1] * 0.5 + 0.5);
  const skyWash = context.createLinearGradient(0, 0, 0, height);
  const hazeAlpha = 0.06 + fogDensity * 0.22;

  skyWash.addColorStop(0, `rgba(238, 243, 249, ${0.08 + daylight * 0.14})`);
  skyWash.addColorStop(0.52, `rgba(196, 209, 224, ${hazeAlpha * 0.58})`);
  skyWash.addColorStop(1, "rgba(7, 12, 15, 0.18)");
  context.fillStyle = skyWash;
  context.fillRect(0, 0, width, height);

  if (fogDensity > 0.01) {
    const vignette = context.createRadialGradient(
      width * 0.5,
      height * 0.48,
      Math.min(width, height) * 0.12,
      width * 0.5,
      height * 0.52,
      Math.max(width, height) * 0.72,
    );
    vignette.addColorStop(0, "rgba(255,255,255,0)");
    vignette.addColorStop(1, `rgba(215, 224, 230, ${fogDensity * 0.22})`);
    context.fillStyle = vignette;
    context.fillRect(0, 0, width, height);
  }
}

export function renderTerrainPreview(canvas, scene) {
  const context = canvas.getContext("2d", { alpha: false });
  const { heights, blocks, view } = scene;
  const settings = scene.settings ?? {};
  const runtime = scene.runtime ?? {};
  const { sampleWidth, sampleHeight } = view;
  const lightDirection = getLightDirection(settings, runtime.dayTime ?? 0.23);
  const fogDensity = clamp(settings.fogDensity ?? 0.24, 0, 1);
  const imageData = context.createImageData(sampleWidth, sampleHeight);
  const pixels = imageData.data;

  let minimum = Number.POSITIVE_INFINITY;
  let maximum = Number.NEGATIVE_INFINITY;

  for (const value of heights) {
    minimum = Math.min(minimum, value);
    maximum = Math.max(maximum, value);
  }

  const range = Math.max(maximum - minimum, 0.0001);
  const daylightGain = 0.78 + clamp(lightDirection[1], -0.2, 1) * 0.22;

  for (let y = 0; y < sampleHeight; y += 1) {
    for (let x = 0; x < sampleWidth; x += 1) {
      const index = y * sampleWidth + x;
      const heightValue = heights[index];
      const normalizedHeight = (heightValue - minimum) / range;
      const shade = computeShade(heights, sampleWidth, sampleHeight, x, y, lightDirection);
      const mist = 1 - Math.abs(normalizedHeight - 0.56) * 1.55;
      const fogLift = fogDensity * 32;
      const baseColor = sampleColorRamp(normalizedHeight);
      const pixelIndex = index * 4;

      pixels[pixelIndex + 0] = Math.min(255, baseColor[0] * shade * daylightGain + 16 * mist + fogLift);
      pixels[pixelIndex + 1] = Math.min(255, baseColor[1] * shade * daylightGain + 14 * mist + fogLift);
      pixels[pixelIndex + 2] = Math.min(255, baseColor[2] * shade * daylightGain + 18 * mist + fogLift * 1.15);
      pixels[pixelIndex + 3] = 255;
    }
  }

  context.putImageData(imageData, 0, 0);
  drawCloudLayer(context, sampleWidth, sampleHeight, settings, runtime);
  drawAtmosphereOverlay(context, sampleWidth, sampleHeight, settings, lightDirection);

  context.save();
  context.lineWidth = 1;
  context.strokeStyle = "rgba(255,255,255,0.12)";

  const gridStep = Math.max(54, Math.round(34 / view.step));
  for (let grid = gridStep; grid < Math.max(sampleWidth, sampleHeight); grid += gridStep) {
    if (grid < sampleWidth) {
      context.beginPath();
      context.moveTo(grid + 0.5, 0);
      context.lineTo(grid + 0.5, sampleHeight);
      context.stroke();
    }

    if (grid < sampleHeight) {
      context.beginPath();
      context.moveTo(0, grid + 0.5);
      context.lineTo(sampleWidth, grid + 0.5);
      context.stroke();
    }
  }

  const markerSize = Math.max(4, Math.round(2.5 / view.step));
  for (const block of blocks) {
    const { pixelX, pixelY } = worldToCanvas(block.x, block.z, view);
    if (
      pixelX < -markerSize ||
      pixelX >= sampleWidth + markerSize ||
      pixelY < -markerSize ||
      pixelY >= sampleHeight + markerSize
    ) {
      continue;
    }

    context.fillStyle = `rgb(${Math.round(block.r * 255)} ${Math.round(block.g * 255)} ${Math.round(block.b * 255)})`;
    context.fillRect(
      Math.round(pixelX - markerSize * 0.5),
      Math.round(pixelY - markerSize * 0.5),
      markerSize,
      markerSize,
    );
  }

  context.strokeStyle = "rgba(244, 247, 250, 0.92)";
  context.beginPath();
  context.moveTo(sampleWidth * 0.5 - 10, sampleHeight * 0.5);
  context.lineTo(sampleWidth * 0.5 + 10, sampleHeight * 0.5);
  context.moveTo(sampleWidth * 0.5, sampleHeight * 0.5 - 10);
  context.lineTo(sampleWidth * 0.5, sampleHeight * 0.5 + 10);
  context.stroke();
  context.restore();
}

export function canvasPointToWorld(canvas, view, clientX, clientY) {
  const bounds = canvas.getBoundingClientRect();
  const scaleX = view.sampleWidth / bounds.width;
  const scaleY = view.sampleHeight / bounds.height;
  const canvasX = (clientX - bounds.left) * scaleX;
  const canvasY = (clientY - bounds.top) * scaleY;

  return {
    x: Math.round(view.centerX + (canvasX - (view.sampleWidth - 1) * 0.5) * view.step),
    z: Math.round(view.centerZ + (canvasY - (view.sampleHeight - 1) * 0.5) * view.step),
  };
}
