const textDecoder = new TextDecoder("utf-8");

export function resolveExport(wasmExports, exportName) {
  return wasmExports[exportName] ?? wasmExports[`_${exportName}`] ?? null;
}

export function withHeapAllocation(malloc, free, byteLength, callback) {
  const pointer = malloc(byteLength);
  if (pointer === 0 && byteLength > 0) {
    throw new Error(`malloc(${byteLength}) failed`);
  }

  try {
    return callback(pointer);
  } finally {
    free(pointer);
  }
}

export function copyFloat32(memory, pointer, length) {
  return new Float32Array(new Float32Array(memory.buffer, pointer, length));
}

export function readCString(memory, pointer) {
  if (!pointer) {
    return "";
  }

  const bytes = new Uint8Array(memory.buffer);
  let end = pointer;
  while (end < bytes.length && bytes[end] !== 0) {
    end += 1;
  }

  return textDecoder.decode(bytes.subarray(pointer, end));
}
