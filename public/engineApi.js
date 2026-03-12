import { loadWasm } from "./helper/loadWasm.js";
import { copyFloat32, readCString, resolveExport, withHeapAllocation } from "./helper/wasmMemory.js";

const FLOAT_BYTES = Float32Array.BYTES_PER_ELEMENT;
const BLOCK_STRIDE_FLOATS = 6;

function requireExport(wasmExports, exportName) {
  const resolved = resolveExport(wasmExports, exportName);
  if (!resolved) {
    throw new Error(`Missing WASM export: ${exportName}`);
  }

  return resolved;
}

export async function createEngineApi(wasmUrl) {
  const wasmExports = await loadWasm(wasmUrl);
  const memory = requireExport(wasmExports, "memory");
  const malloc = requireExport(wasmExports, "malloc");
  const free = requireExport(wasmExports, "free");
  const resetState = requireExport(wasmExports, "engine_web_reset_state");
  const setTerrainProfile = requireExport(wasmExports, "engine_web_set_terrain_profile");
  const fillHeightBuffer = requireExport(wasmExports, "engine_web_fill_height_buffer");
  const getBlockCount = requireExport(wasmExports, "engine_web_get_block_count");
  const copyBlocks = requireExport(wasmExports, "engine_web_copy_blocks");
  const placeBlockColumn = requireExport(wasmExports, "engine_web_place_block_column");
  const removeBlockColumn = requireExport(wasmExports, "engine_web_remove_block_column");
  const describeState = requireExport(wasmExports, "engine_web_describe_state");

  return {
    resetState() {
      resetState();
    },

    setTerrainProfile(profile) {
      setTerrainProfile(
        Number(profile.baseHeight),
        Number(profile.heightScale),
        Number(profile.roughness),
        Number(profile.ridgeStrength),
      );
    },

    sampleTerrain(view) {
      const sampleCount = view.sampleWidth * view.sampleHeight;
      const byteLength = sampleCount * FLOAT_BYTES;

      return withHeapAllocation(malloc, free, byteLength, (pointer) => {
        const written = fillHeightBuffer(
          view.sampleWidth,
          view.sampleHeight,
          view.centerX,
          view.centerZ,
          view.step,
          pointer,
        );

        if (written !== sampleCount) {
          throw new Error(`Unexpected terrain sample count: ${written}`);
        }

        return copyFloat32(memory, pointer, sampleCount);
      });
    },

    getBlocks() {
      const blockCount = Math.max(getBlockCount(), 1);
      const byteLength = blockCount * BLOCK_STRIDE_FLOATS * FLOAT_BYTES;

      return withHeapAllocation(malloc, free, byteLength, (pointer) => {
        const copied = copyBlocks(pointer, blockCount);
        const raw = copyFloat32(memory, pointer, copied * BLOCK_STRIDE_FLOATS);
        const blocks = [];

        for (let index = 0; index < copied; index += 1) {
          const baseIndex = index * BLOCK_STRIDE_FLOATS;
          blocks.push({
            x: raw[baseIndex + 0],
            y: raw[baseIndex + 1],
            z: raw[baseIndex + 2],
            r: raw[baseIndex + 3],
            g: raw[baseIndex + 4],
            b: raw[baseIndex + 5],
          });
        }

        return blocks;
      });
    },

    placeBlock(x, z, blockType) {
      return placeBlockColumn(Math.round(x), Math.round(z), blockType);
    },

    removeBlock(x, z) {
      return removeBlockColumn(Math.round(x), Math.round(z));
    },

    getStateSummary() {
      const pointer = describeState();
      if (!pointer) {
        return "";
      }

      try {
        return readCString(memory, pointer);
      } finally {
        free(pointer);
      }
    },
  };
}
