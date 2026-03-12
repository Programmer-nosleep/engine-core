import { createEngineApi } from "./engineApi.js";
import { canvasPointToWorld, renderTerrainPreview } from "./terrainPreview.js";

const WASM_URL = "public/wasm/engine-core.wasm";
const BLOCK_LABELS = { 1: "Grass", 2: "Stone", 3: "Wood", 4: "Glow" };
const DEFAULT_SCENE = {
  sunDistance: 149.6,
  sunOrbit: 82.8,
  cycleDuration: 180,
  daylightFraction: 0.5,
  cameraFov: 65,
  fogDensity: 0.24,
  cloudAmount: 0.54,
  cloudSpacing: 1,
  terrainBase: -14,
  terrainHeight: 1,
  terrainRoughness: 1,
  terrainRidge: 1,
  palmSize: 1,
  palmCount: 24,
  palmFruitDensity: 0.55,
  palmRenderRadius: 260,
  cloudsEnabled: true,
  freezeTime: false,
  godMode: true,
  gpuMode: "auto",
};
const DEFAULT_VIEW = { centerX: 0, centerZ: 4, step: 0.45, sampleWidth: 960, sampleHeight: 540 };
const TERRAIN_KEYS = new Set(["terrainBase", "terrainHeight", "terrainRoughness", "terrainRidge"]);
const VISUAL_KEYS = new Set(["sunDistance", "sunOrbit", "daylightFraction", "fogDensity", "cloudAmount", "cloudSpacing"]);
const sliderDefinitions = [
  { key: "sunDistance", format: (value) => `${value.toFixed(1)} Mkm` },
  { key: "sunOrbit", format: (value) => `${value.toFixed(1)} deg` },
  { key: "cycleDuration", format: (value) => `${Math.round(value)} s` },
  { key: "daylightFraction", format: (value) => `${Math.round(value * 100)}%` },
  { key: "cameraFov", format: (value) => `${Math.round(value)} deg` },
  { key: "fogDensity", format: (value) => `${Math.round(value * 100)}%` },
  { key: "cloudAmount", format: (value) => `${Math.round(value * 100)}%` },
  { key: "cloudSpacing", format: (value) => `${value.toFixed(2)}x` },
  { key: "terrainBase", format: (value) => value.toFixed(1) },
  { key: "terrainHeight", format: (value) => `${value.toFixed(2)}x` },
  { key: "terrainRoughness", format: (value) => `${value.toFixed(2)}x` },
  { key: "terrainRidge", format: (value) => `${value.toFixed(2)}x` },
  { key: "palmSize", format: (value) => `${value.toFixed(2)}x` },
  { key: "palmCount", format: (value) => `${Math.round(value)}` },
  { key: "palmFruitDensity", format: (value) => `${Math.round(value * 100)}%` },
  { key: "palmRenderRadius", format: (value) => `${Math.round(value)} m` },
];

const byId = (id) => document.getElementById(id);
const elements = {
  body: document.body,
  viewportShell: byId("viewportShell"),
  windowTitle: byId("windowTitle"),
  canvas: byId("terrainPreview"),
  summaryText: byId("summaryText"),
  statusText: byId("statusText"),
  viewState: byId("viewState"),
  hoverState: byId("hoverState"),
  overlayStateText: byId("overlayStateText"),
  overlayHintText: byId("overlayHintText"),
  toggleOverlayPanel: byId("toggleOverlayPanel"),
  toggleOverlayGlyph: byId("toggleOverlayGlyph"),
  playerModeValue: byId("playerModeValue"),
  timeModeValue: byId("timeModeValue"),
  cloudModeValue: byId("cloudModeValue"),
  blockPalette: byId("blockPalette"),
  resetScene: byId("resetScene"),
  recenterView: byId("recenterView"),
  panNorth: byId("panNorth"),
  panWest: byId("panWest"),
  panEast: byId("panEast"),
  panSouth: byId("panSouth"),
  zoomIn: byId("zoomIn"),
  zoomOut: byId("zoomOut"),
  godMode: byId("godMode"),
  freezeTime: byId("freezeTime"),
  cloudsEnabled: byId("cloudsEnabled"),
  gpuRouting: byId("gpuRouting"),
  gpuPowerText: byId("gpuPowerText"),
  gpuHighText: byId("gpuHighText"),
  gpuRendererText: byId("gpuRendererText"),
  hudRenderer: byId("hudRenderer"),
  hudGpu0: byId("hudGpu0"),
  hudGpu1: byId("hudGpu1"),
  hudCpu: byId("hudCpu"),
  hudFps: byId("hudFps"),
  hudFrameMs: byId("hudFrameMs"),
  hudFrameExtents: byId("hudFrameExtents"),
  hudResolution: byId("hudResolution"),
  hudFov: byId("hudFov"),
  hudMode: byId("hudMode"),
  hudBlock: byId("hudBlock"),
  hudFogCloud: byId("hudFogCloud"),
  hudTarget: byId("hudTarget"),
  frameGraph: byId("frameGraph"),
  positionLine: byId("positionLine"),
  cellLine: byId("cellLine"),
  diagnosticsLog: byId("diagnosticsLog"),
};

const view = { ...DEFAULT_VIEW };
const sceneState = { ...DEFAULT_SCENE };
const runtime = {
  dayTime: 0.23,
  elapsedSeconds: 0,
  previousTick: performance.now(),
  frameHistory: [],
  frameMs: 0,
  frameMinMs: 0,
  frameMaxMs: 0,
  fps: 0,
  lastRenderDurationMs: 0,
  lastSampleDurationMs: 0,
  lastCenterHeight: 0,
  lastSummary: "",
  interactionPulse: 0,
  pointerInside: false,
  overlayCollapsed: false,
  usingFallback: false,
  targetActive: true,
  diagnostics: [],
  lastAtmosphereRender: 0,
  atmosphereDirty: true,
};

let engineApi = null;
let selectedBlockType = 1;
let refreshQueued = false;
let cachedHeights = null;
let cachedBlocks = [];
let resizeHandle = 0;

function clamp(value, minimum, maximum) {
  return Math.min(Math.max(value, minimum), maximum);
}

function mixColor(from, to, factor) {
  return from.map((value, index) => value + (to[index] - value) * factor);
}

function rgbString(values) {
  return `rgb(${values.map((value) => Math.round(value)).join(" ")})`;
}

function formatWorldClock(dayTime) {
  const wrapped = ((dayTime % 1) + 1) % 1;
  const totalMinutes = Math.floor(wrapped * 24 * 60 + 0.5) % (24 * 60);
  const hours = Math.floor(totalMinutes / 60);
  const minutes = totalMinutes % 60;
  return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}`;
}

function formatTimestamp(date) {
  const pad = (value, length = 2) => String(value).padStart(length, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}.${pad(date.getMilliseconds(), 3)}`;
}

function getSelectedBlockLabel() {
  return BLOCK_LABELS[selectedBlockType] ?? BLOCK_LABELS[1];
}

function addDiagnostics(message) {
  runtime.diagnostics.push(`[${formatTimestamp(new Date())}] ${message}`);
  if (runtime.diagnostics.length > 8) {
    runtime.diagnostics.shift();
  }
  if (elements.diagnosticsLog) {
    elements.diagnosticsLog.textContent = runtime.diagnostics.join("\n");
  }
}

function setStatus(message, isError = false, log = false) {
  if (elements.statusText) {
    elements.statusText.textContent = message;
    elements.statusText.style.color = isError ? "#ffb4a2" : "";
  }
  if (isError) {
    console.error(message);
  }
  if (log) {
    addDiagnostics(`browser_preview: ${message}`);
  }
}

function measureInteraction(amount = 8) {
  runtime.interactionPulse = clamp(runtime.interactionPulse + amount, 0, 72);
}

function calculateDaylightStrength() {
  const sunrise = 0.5 - sceneState.daylightFraction * 0.5;
  const sunset = 0.5 + sceneState.daylightFraction * 0.5;
  const wrapped = ((runtime.dayTime % 1) + 1) % 1;
  if (wrapped < sunrise || wrapped > sunset) {
    return 0.14;
  }
  const daylightT = (wrapped - sunrise) / Math.max(sunset - sunrise, 0.001);
  return 0.18 + Math.sin(daylightT * Math.PI) * 0.82;
}

function applyViewportAtmosphere() {
  if (!elements.viewportShell) {
    return;
  }
  const daylight = calculateDaylightStrength();
  const fog = sceneState.fogDensity;
  const skyTop = mixColor([20, 30, 49], [154, 193, 200], daylight);
  const skyBottom = mixColor([54, 65, 98], [207, 201, 202], daylight * (1 - fog * 0.18));
  const ground = mixColor([49, 63, 43], [99, 129, 79], daylight * (1 - fog * 0.22));
  const glowAlpha = clamp(0.08 + daylight * 0.14 + sceneState.cloudAmount * 0.02, 0.08, 0.26);
  elements.viewportShell.style.setProperty("--sky-top", rgbString(skyTop));
  elements.viewportShell.style.setProperty("--sky-bottom", rgbString(skyBottom));
  elements.viewportShell.style.setProperty("--ground-tone", rgbString(ground));
  elements.viewportShell.style.setProperty("--atmosphere-glow", `rgba(255, 229, 168, ${glowAlpha.toFixed(3)})`);
}

function syncCanvasResolution(force = false) {
  if (!elements.canvas) {
    return false;
  }
  const bounds = elements.canvas.getBoundingClientRect();
  const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
  const width = clamp(Math.round((bounds.width || 960) * dpr), 480, 960);
  const height = clamp(Math.round((bounds.height || 540) * dpr), 270, 540);
  if (!force && width === view.sampleWidth && height === view.sampleHeight) {
    return false;
  }
  view.sampleWidth = width;
  view.sampleHeight = height;
  elements.canvas.width = width;
  elements.canvas.height = height;
  return true;
}

function getTerrainProfile() {
  return {
    baseHeight: sceneState.terrainBase,
    heightScale: sceneState.terrainHeight,
    roughness: sceneState.terrainRoughness,
    ridgeStrength: sceneState.terrainRidge,
  };
}

function buildFallbackTerrain() {
  const heights = new Float32Array(view.sampleWidth * view.sampleHeight);
  for (let row = 0; row < view.sampleHeight; row += 1) {
    for (let column = 0; column < view.sampleWidth; column += 1) {
      const worldX = view.centerX + (column - (view.sampleWidth - 1) * 0.5) * view.step;
      const worldZ = view.centerZ + (row - (view.sampleHeight - 1) * 0.5) * view.step;
      const rolling = Math.sin(worldX * 0.085) * 4.8 * sceneState.terrainHeight + Math.cos(worldZ * 0.06) * 3.6 * sceneState.terrainHeight;
      const rough = Math.sin((worldX + worldZ) * 0.15) * sceneState.terrainRoughness * 1.6 + Math.cos(worldX * 0.19 - worldZ * 0.11) * sceneState.terrainRoughness * 1.1;
      const ridge = Math.abs(Math.sin(worldX * 0.05) * Math.cos(worldZ * 0.045)) * sceneState.terrainRidge * 6.8;
      heights[row * view.sampleWidth + column] = sceneState.terrainBase + rolling + rough + ridge;
    }
  }
  return heights;
}

function updateOutputs() {
  for (const definition of sliderDefinitions) {
    const input = byId(definition.key);
    const output = byId(`${definition.key}Value`);
    if (input) {
      input.value = sceneState[definition.key];
    }
    if (output) {
      output.textContent = definition.format(sceneState[definition.key]);
    }
  }
}

function updateToggleState() {
  if (elements.godMode) {
    elements.godMode.checked = sceneState.godMode;
  }
  if (elements.freezeTime) {
    elements.freezeTime.checked = sceneState.freezeTime;
  }
  if (elements.cloudsEnabled) {
    elements.cloudsEnabled.checked = sceneState.cloudsEnabled;
  }
  if (elements.playerModeValue) {
    elements.playerModeValue.textContent = sceneState.godMode ? "God" : "Survival";
  }
  if (elements.timeModeValue) {
    elements.timeModeValue.textContent = sceneState.freezeTime ? "Frozen" : "Running";
  }
  if (elements.cloudModeValue) {
    elements.cloudModeValue.textContent = sceneState.cloudsEnabled ? "Enabled" : "Disabled";
  }
}

function updateGpuState() {
  if (elements.gpuRouting) {
    for (const button of elements.gpuRouting.querySelectorAll("[data-gpu-mode]")) {
      button.classList.toggle("is-active", button.dataset.gpuMode === sceneState.gpuMode);
    }
  }
  if (elements.gpuPowerText) {
    elements.gpuPowerText.textContent = sceneState.gpuMode === "eco"
      ? "Power: Browser default adapter"
      : "Power: Browser default adapter (boost)";
  }
  if (elements.gpuHighText) {
    elements.gpuHighText.textContent = sceneState.gpuMode === "high"
      ? "High: Browser compositor preferred"
      : "High: Browser compositor";
  }
  if (elements.gpuRendererText) {
    elements.gpuRendererText.textContent = runtime.usingFallback
      ? "Renderer: JS fallback raster"
      : "Renderer: WASM terrain sampler + Canvas 2D";
  }
}

function updateWindowTitle() {
  if (!elements.windowTitle) {
    return;
  }
  const cycleState = sceneState.freezeTime ? "Frozen" : "Auto";
  const playerMode = sceneState.godMode ? "Creative" : "Survival";
  elements.windowTitle.textContent =
    `OpenGL Sky | ${formatWorldClock(runtime.dayTime)} | ${cycleState} | ${playerMode} | ` +
    `block ${getSelectedBlockLabel()} | cycle ${Math.round(sceneState.cycleDuration)}s | ` +
    "G mode | 1-4 blocks | LMB break | RMB place | Alt overlay";
}

function updateOverlayState() {
  if (!elements.body) {
    return;
  }
  elements.body.classList.toggle("overlay-collapsed", runtime.overlayCollapsed);
  if (elements.toggleOverlayGlyph) {
    elements.toggleOverlayGlyph.textContent = runtime.overlayCollapsed ? "\u203a" : "\u2039";
  }
  if (elements.overlayStateText) {
    elements.overlayStateText.textContent = runtime.pointerInside ? "Cursor active" : "Overlay locked";
  }
  if (elements.overlayHintText) {
    elements.overlayHintText.textContent = runtime.pointerInside
      ? "LMB break, RMB place, wheel zoom, and number keys switch blocks."
      : "Use the browser panel to shape the scene and the canvas to edit the world.";
  }
}

function updateViewLabel() {
  if (elements.viewState) {
    elements.viewState.textContent = `center (${view.centerX.toFixed(1)}, ${view.centerZ.toFixed(1)}) | step ${view.step.toFixed(2)}`;
  }
}

function updateHoverLabel(world = null) {
  if (!elements.hoverState) {
    return;
  }
  elements.hoverState.textContent = world ? `hover ${world.x}, ${world.z}` : "hover --";
}

function updateConsolePosition() {
  const positionY = runtime.lastCenterHeight + 1.62;
  if (elements.positionLine) {
    elements.positionLine.textContent = `POS X ${view.centerX.toFixed(2)} Y ${positionY.toFixed(2)} Z ${view.centerZ.toFixed(2)}`;
  }
  if (elements.cellLine) {
    elements.cellLine.textContent = `CELL X ${Math.floor(view.centerX)} Y ${Math.floor(positionY)} Z ${Math.floor(view.centerZ)}`;
  }
}

function buildSummaryText() {
  const baseSummary = runtime.lastSummary || "Browser preview active.";
  return `${baseSummary} | palms ${Math.round(sceneState.palmCount)} | clouds ${sceneState.cloudsEnabled ? "on" : "off"}`;
}

function renderPreview() {
  if (!elements.canvas || !cachedHeights) {
    return;
  }
  applyViewportAtmosphere();
  const renderStart = performance.now();
  renderTerrainPreview(elements.canvas, {
    heights: cachedHeights,
    blocks: cachedBlocks,
    view,
    settings: sceneState,
    runtime,
  });
  runtime.lastRenderDurationMs = performance.now() - renderStart;
  runtime.lastAtmosphereRender = performance.now();
  const centerIndex = Math.floor(view.sampleHeight * 0.5) * view.sampleWidth + Math.floor(view.sampleWidth * 0.5);
  runtime.lastCenterHeight = cachedHeights[centerIndex] ?? runtime.lastCenterHeight;
  runtime.targetActive = true;
  if (elements.summaryText) {
    elements.summaryText.textContent = buildSummaryText();
  }
  updateConsolePosition();
  updateViewLabel();
  updateHud();
}

function refreshScene() {
  syncCanvasResolution();
  const sampleStart = performance.now();
  try {
    if (engineApi) {
      engineApi.setTerrainProfile(getTerrainProfile());
      cachedHeights = engineApi.sampleTerrain(view);
      cachedBlocks = engineApi.getBlocks();
      runtime.lastSummary = engineApi.getStateSummary();
      runtime.usingFallback = false;
    } else {
      cachedHeights = buildFallbackTerrain();
      cachedBlocks = [];
      runtime.lastSummary = "WASM unavailable. Browser fallback terrain is showing procedural hills only.";
      runtime.usingFallback = true;
    }
    runtime.lastSampleDurationMs = performance.now() - sampleStart;
    runtime.atmosphereDirty = false;
    renderPreview();
    setStatus(
      engineApi
        ? `Loaded ${cachedBlocks.length} block markers from ${WASM_URL}`
        : "WASM belum aktif. Browser memakai fallback terrain preview.",
      !engineApi,
    );
    updateGpuState();
  } catch (error) {
    console.error(error);
    engineApi = null;
    cachedHeights = buildFallbackTerrain();
    cachedBlocks = [];
    runtime.lastSampleDurationMs = performance.now() - sampleStart;
    runtime.usingFallback = true;
    runtime.lastSummary = "WASM refresh failed. Browser fallback terrain remains active.";
    renderPreview();
    setStatus("Gagal refresh wasm. Browser fallback terrain sedang dipakai.", true, true);
  }
}

function scheduleRefresh(message = "Recomputing browser scene...", log = false) {
  setStatus(message, false, log);
  if (refreshQueued) {
    return;
  }
  refreshQueued = true;
  requestAnimationFrame(() => {
    refreshQueued = false;
    refreshScene();
  });
}

function setSelectedPalette(button) {
  if (!elements.blockPalette) {
    return;
  }
  for (const chip of elements.blockPalette.querySelectorAll(".palette-chip")) {
    chip.classList.toggle("is-active", chip === button);
  }
}

function updateHud() {
  const blockCount = cachedBlocks.length;
  const avgFrame = runtime.frameMs || (runtime.lastRenderDurationMs + runtime.lastSampleDurationMs);
  const gpuBase = sceneState.gpuMode === "eco" ? 12 : sceneState.gpuMode === "high" ? 30 : 20;
  const visualLoad = sceneState.cloudsEnabled ? sceneState.cloudAmount * 20 : 2;
  const zoomLoad = clamp(12 / view.step, 0, 28);
  const gpu0Usage = clamp(gpuBase + runtime.lastRenderDurationMs * 14 + zoomLoad + visualLoad + runtime.interactionPulse * 0.6, 0, 99);
  const gpu1Usage = sceneState.gpuMode === "high" ? clamp(gpu0Usage * 0.12, 0, 22) : 0;
  const cpuUsage = clamp(12 + runtime.lastSampleDurationMs * 9 + runtime.lastRenderDurationMs * 7 + runtime.interactionPulse * 0.35, 0, 98);

  if (elements.hudRenderer) elements.hudRenderer.textContent = runtime.usingFallback ? "JS" : "WASM";
  if (elements.hudGpu0) elements.hudGpu0.textContent = `${Math.round(gpu0Usage)}%`;
  if (elements.hudGpu1) elements.hudGpu1.textContent = `${Math.round(gpu1Usage)}%`;
  if (elements.hudCpu) elements.hudCpu.textContent = `${Math.round(cpuUsage)}%`;
  if (elements.hudFps) elements.hudFps.textContent = `${Math.max(1, Math.round(runtime.fps || 60))}`;
  if (elements.hudFrameMs) elements.hudFrameMs.textContent = avgFrame.toFixed(1);
  if (elements.hudFrameExtents) elements.hudFrameExtents.textContent = `min: ${runtime.frameMinMs.toFixed(1)}ms, max: ${runtime.frameMaxMs.toFixed(1)}ms`;
  if (elements.hudResolution) elements.hudResolution.textContent = `RES ${view.sampleWidth}x${view.sampleHeight}`;
  if (elements.hudFov) elements.hudFov.textContent = `FOV ${Math.round(sceneState.cameraFov)}`;
  if (elements.hudMode) elements.hudMode.textContent = `MODE ${sceneState.godMode ? "Creative" : "Survival"}`;
  if (elements.hudBlock) elements.hudBlock.textContent = `BLK ${getSelectedBlockLabel()} ${String(blockCount).padStart(3, "0")}`;
  if (elements.hudFogCloud) elements.hudFogCloud.textContent = `FOG ${Math.round(sceneState.fogDensity * 100)}% CLD ${sceneState.cloudsEnabled ? "ON" : "OFF"}`;
  if (elements.hudTarget) elements.hudTarget.textContent = `TGT ${runtime.targetActive ? "ON" : "OFF"}`;
}

function drawFrameGraph() {
  if (!elements.frameGraph) {
    return;
  }
  const context = elements.frameGraph.getContext("2d");
  const width = elements.frameGraph.width;
  const height = elements.frameGraph.height;
  const history = runtime.frameHistory;
  context.clearRect(0, 0, width, height);
  context.fillStyle = "rgba(4, 10, 8, 0.92)";
  context.fillRect(0, 0, width, height);
  context.strokeStyle = "rgba(53, 88, 61, 0.35)";
  for (let row = 1; row <= 3; row += 1) {
    const y = (height / 4) * row + 0.5;
    context.beginPath();
    context.moveTo(0, y);
    context.lineTo(width, y);
    context.stroke();
  }
  if (history.length < 2) {
    return;
  }
  const displayMax = clamp(Math.max(...history) * 1.2, 18, 66);
  for (const [lineWidth, color] of [[3, "rgba(27, 255, 84, 0.24)"], [1.5, "rgba(19, 245, 67, 0.92)"]]) {
    context.lineWidth = lineWidth;
    context.strokeStyle = color;
    context.beginPath();
    history.forEach((value, index) => {
      const x = (index / (history.length - 1)) * width;
      const y = height - clamp(value / displayMax, 0, 1) * height;
      if (index === 0) {
        context.moveTo(x, y);
      } else {
        context.lineTo(x, y);
      }
    });
    context.stroke();
  }
  const lastValue = history[history.length - 1];
  const markerY = height - clamp(lastValue / displayMax, 0, 1) * height;
  context.fillStyle = "rgba(208, 255, 216, 0.96)";
  context.fillRect(width - 3, markerY - 2, 4, 4);
}

function moveView(deltaX, deltaZ) {
  view.centerX += deltaX;
  view.centerZ += deltaZ;
  measureInteraction(10);
  scheduleRefresh("Shifting viewport...");
}

function zoomView(multiplier) {
  view.step = clamp(view.step * multiplier, 0.12, 1.5);
  measureInteraction(8);
  scheduleRefresh("Zooming scene preview...");
}

function resetState() {
  Object.assign(sceneState, DEFAULT_SCENE);
  Object.assign(view, DEFAULT_VIEW);
  runtime.dayTime = 0.23;
  runtime.lastSummary = "";
  runtime.atmosphereDirty = true;
  selectedBlockType = 1;
  updateOutputs();
  updateToggleState();
  updateGpuState();
  applyViewportAtmosphere();
  const firstPaletteButton = elements.blockPalette?.querySelector("[data-block-type='1']");
  if (firstPaletteButton) {
    setSelectedPalette(firstPaletteButton);
  }
}

function bindControls() {
  for (const definition of sliderDefinitions) {
    const input = byId(definition.key);
    if (!input) {
      continue;
    }
    input.addEventListener("input", () => {
      sceneState[definition.key] = Number(input.value);
      updateOutputs();
      updateWindowTitle();
      applyViewportAtmosphere();
      measureInteraction(4);
      if (TERRAIN_KEYS.has(definition.key)) {
        scheduleRefresh("Updating wasm terrain profile...");
        return;
      }
      if (VISUAL_KEYS.has(definition.key)) {
        runtime.atmosphereDirty = true;
        renderPreview();
      } else {
        updateHud();
      }
    });
  }

  elements.toggleOverlayPanel?.addEventListener("click", () => {
    runtime.overlayCollapsed = !runtime.overlayCollapsed;
    updateOverlayState();
  });

  elements.godMode?.addEventListener("change", () => {
    sceneState.godMode = elements.godMode.checked;
    updateToggleState();
    updateWindowTitle();
    updateHud();
    addDiagnostics(`player_mode: ${sceneState.godMode ? "creative" : "survival"}`);
  });

  elements.freezeTime?.addEventListener("change", () => {
    sceneState.freezeTime = elements.freezeTime.checked;
    updateToggleState();
    updateWindowTitle();
    measureInteraction(5);
    addDiagnostics(`time_controls: ${sceneState.freezeTime ? "frozen" : "running"}`);
  });

  elements.cloudsEnabled?.addEventListener("change", () => {
    sceneState.cloudsEnabled = elements.cloudsEnabled.checked;
    updateToggleState();
    updateWindowTitle();
    runtime.atmosphereDirty = true;
    renderPreview();
    addDiagnostics(`cloud_controls: ${sceneState.cloudsEnabled ? "enabled" : "disabled"}`);
  });

  elements.gpuRouting?.addEventListener("click", (event) => {
    const button = event.target.closest("[data-gpu-mode]");
    if (!button) {
      return;
    }
    sceneState.gpuMode = button.dataset.gpuMode;
    updateGpuState();
    updateHud();
    measureInteraction(6);
    addDiagnostics(`gpu_routing: ${sceneState.gpuMode}`);
  });

  elements.blockPalette?.addEventListener("click", (event) => {
    const button = event.target.closest("[data-block-type]");
    if (!button) {
      return;
    }
    selectedBlockType = Number(button.dataset.blockType);
    setSelectedPalette(button);
    updateWindowTitle();
    updateHud();
    addDiagnostics(`block_palette: ${getSelectedBlockLabel().toLowerCase()}`);
  });

  elements.resetScene?.addEventListener("click", () => {
    if (engineApi) {
      engineApi.resetState();
    }
    resetState();
    syncCanvasResolution(true);
    scheduleRefresh("Resetting scene state...", true);
  });

  elements.recenterView?.addEventListener("click", () => {
    view.centerX = 0;
    view.centerZ = 4;
    measureInteraction(6);
    scheduleRefresh("Returning to spawn marker...", true);
  });

  elements.panNorth?.addEventListener("click", () => moveView(0, -24));
  elements.panWest?.addEventListener("click", () => moveView(-24, 0));
  elements.panEast?.addEventListener("click", () => moveView(24, 0));
  elements.panSouth?.addEventListener("click", () => moveView(0, 24));
  elements.zoomIn?.addEventListener("click", () => zoomView(0.82));
  elements.zoomOut?.addEventListener("click", () => zoomView(1.18));

  if (!elements.canvas || elements.canvas.dataset.interactive !== "true") {
    return;
  }

  elements.canvas.addEventListener("contextmenu", (event) => event.preventDefault());
  elements.canvas.addEventListener("pointerenter", () => {
    runtime.pointerInside = true;
    updateOverlayState();
  });
  elements.canvas.addEventListener("pointerleave", () => {
    runtime.pointerInside = false;
    updateOverlayState();
    updateHoverLabel(null);
  });
  elements.canvas.addEventListener("wheel", (event) => {
    event.preventDefault();
    zoomView(event.deltaY < 0 ? 0.9 : 1.1);
  }, { passive: false });
  elements.canvas.addEventListener("pointermove", (event) => {
    updateHoverLabel(canvasPointToWorld(elements.canvas, view, event.clientX, event.clientY));
  });
  elements.canvas.addEventListener("pointerdown", (event) => {
    const world = canvasPointToWorld(elements.canvas, view, event.clientX, event.clientY);
    measureInteraction(12);
    if (!engineApi) {
      setStatus("Block editing perlu wasm aktif. Fallback terrain hanya read-only.", true, true);
      return;
    }
    if (event.button === 0) {
      engineApi.removeBlock(world.x, world.z);
      addDiagnostics(`block_remove: column at ${world.x}, ${world.z}`);
      scheduleRefresh(`Removed block at ${world.x}, ${world.z}`);
      return;
    }
    if (event.button === 2) {
      engineApi.placeBlock(world.x, world.z, selectedBlockType);
      addDiagnostics(`block_place: ${getSelectedBlockLabel().toLowerCase()} at ${world.x}, ${world.z}`);
      scheduleRefresh(`Placed block at ${world.x}, ${world.z}`);
    }
  });

  window.addEventListener("resize", () => {
    window.clearTimeout(resizeHandle);
    resizeHandle = window.setTimeout(() => {
      if (syncCanvasResolution()) {
        scheduleRefresh("Resizing browser viewport...");
      }
    }, 80);
  });

  window.addEventListener("keydown", (event) => {
    const target = event.target;
    if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
      return;
    }
    if (event.key >= "1" && event.key <= "4") {
      const type = Number(event.key);
      const button = elements.blockPalette?.querySelector(`[data-block-type='${type}']`);
      if (button) {
        selectedBlockType = type;
        setSelectedPalette(button);
        updateWindowTitle();
        updateHud();
      }
      return;
    }
    switch (event.key) {
      case "g":
      case "G":
        sceneState.godMode = !sceneState.godMode;
        updateToggleState();
        updateWindowTitle();
        updateHud();
        break;
      case "r":
      case "R":
        if (engineApi) {
          engineApi.resetState();
        }
        resetState();
        syncCanvasResolution(true);
        scheduleRefresh("Resetting scene state...");
        break;
      case "ArrowUp":
        moveView(0, -12);
        break;
      case "ArrowLeft":
        moveView(-12, 0);
        break;
      case "ArrowRight":
        moveView(12, 0);
        break;
      case "ArrowDown":
        moveView(0, 12);
        break;
      case "+":
      case "=":
        zoomView(0.88);
        break;
      case "-":
      case "_":
        zoomView(1.12);
        break;
      default:
        break;
    }
  });
}

function tick(now) {
  const deltaMs = clamp(now - runtime.previousTick, 4, 250);
  runtime.previousTick = now;
  runtime.elapsedSeconds += deltaMs / 1000;
  if (!sceneState.freezeTime) {
    runtime.dayTime = (runtime.dayTime + (deltaMs / 1000) / sceneState.cycleDuration) % 1;
  }
  runtime.frameHistory.push(deltaMs);
  if (runtime.frameHistory.length > 120) {
    runtime.frameHistory.shift();
  }
  const sum = runtime.frameHistory.reduce((total, value) => total + value, 0);
  runtime.frameMs = runtime.frameHistory.length > 0 ? sum / runtime.frameHistory.length : deltaMs;
  runtime.frameMinMs = runtime.frameHistory.length > 0 ? Math.min(...runtime.frameHistory) : runtime.frameMs;
  runtime.frameMaxMs = runtime.frameHistory.length > 0 ? Math.max(...runtime.frameHistory) : runtime.frameMs;
  runtime.fps = runtime.frameMs > 0 ? 1000 / runtime.frameMs : 0;
  runtime.interactionPulse = Math.max(0, runtime.interactionPulse - 0.7);
  updateWindowTitle();
  updateConsolePosition();
  updateHud();
  drawFrameGraph();
  if (cachedHeights && (!sceneState.freezeTime || runtime.atmosphereDirty) && now - runtime.lastAtmosphereRender > 280) {
    runtime.atmosphereDirty = false;
    renderPreview();
  }
  requestAnimationFrame(tick);
}

(async () => {
  if (!elements.canvas) {
    console.error('Canvas element "#terrainPreview" tidak ditemukan.');
    return;
  }

  if (location.protocol === "file:") {
    setStatus("Buka lewat server lokal, bukan file://, supaya fetch WASM berhasil.", true, true);
  }

  resetState();
  bindControls();
  syncCanvasResolution(true);
  updateOverlayState();
  updateOutputs();
  updateToggleState();
  updateGpuState();
  updateWindowTitle();
  updateViewLabel();
  updateHoverLabel(null);
  addDiagnostics("browser_preview: initializing runtime");

  try {
    engineApi = await createEngineApi(WASM_URL);
    engineApi.resetState();
    addDiagnostics("browser_preview: wasm module loaded");
    refreshScene();
  } catch (error) {
    console.error(error);
    engineApi = null;
    addDiagnostics("browser_preview: wasm load failed, using fallback terrain");
    cachedHeights = buildFallbackTerrain();
    cachedBlocks = [];
    runtime.usingFallback = true;
    runtime.lastSummary = "WASM module belum tersedia. Browser fallback terrain aktif.";
    renderPreview();
    setStatus("Build dengan preset emscripten-wasm lalu jalankan lewat HTTP server.", true);
  }

  requestAnimationFrame(tick);
})();
