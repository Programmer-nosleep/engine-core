function mergeImports(baseImports, extraImports) {
  const merged = { ...baseImports };

  for (const [moduleName, moduleValue] of Object.entries(extraImports)) {
    merged[moduleName] = {
      ...(merged[moduleName] ?? {}),
      ...moduleValue,
    };
  }

  return merged;
}

function createStandaloneImports() {
  const textDecoder = new TextDecoder("utf-8");
  let memoryRef = null;

  const getView = () => (memoryRef ? new DataView(memoryRef.buffer) : null);
  const getBytes = () => (memoryRef ? new Uint8Array(memoryRef.buffer) : null);

  const writeU32 = (pointer, value) => {
    const view = getView();
    if (!view) {
      return 0;
    }

    view.setUint32(pointer, value >>> 0, true);
    return 0;
  };

  const writeU64 = (pointer, value) => {
    const view = getView();
    if (!view) {
      return 0;
    }

    if (typeof view.setBigUint64 === "function") {
      view.setBigUint64(pointer, value, true);
      return 0;
    }

    view.setUint32(pointer, Number(value & 0xffffffffn), true);
    view.setUint32(pointer + 4, Number((value >> 32n) & 0xffffffffn), true);
    return 0;
  };

  const wasi = {
    args_get: () => 0,
    args_sizes_get: (argcPointer, argvBufferSizePointer) => {
      writeU32(argcPointer, 0);
      writeU32(argvBufferSizePointer, 0);
      return 0;
    },
    environ_get: () => 0,
    environ_sizes_get: (countPointer, bufferSizePointer) => {
      writeU32(countPointer, 0);
      writeU32(bufferSizePointer, 0);
      return 0;
    },
    clock_time_get: (_clockId, _precision, outputPointer) => {
      writeU64(outputPointer, BigInt(Date.now()) * 1000000n);
      return 0;
    },
    fd_close: (fd) => (fd === 0 || fd === 1 || fd === 2 ? 0 : 8),
    fd_seek: () => 29,
    fd_write: (fd, iovsPointer, iovsLength, writtenPointer) => {
      const view = getView();
      const bytes = getBytes();

      if (!view || !bytes) {
        return 8;
      }
      if (fd !== 1 && fd !== 2) {
        return 8;
      }

      let written = 0;
      let text = "";

      for (let index = 0; index < iovsLength; index += 1) {
        const pointer = view.getUint32(iovsPointer + index * 8, true);
        const length = view.getUint32(iovsPointer + index * 8 + 4, true);
        text += textDecoder.decode(bytes.subarray(pointer, pointer + length));
        written += length;
      }

      view.setUint32(writtenPointer, written, true);
      if (fd === 2) {
        console.error(text);
      } else if (text.trim() !== "") {
        console.log(text);
      }

      return 0;
    },
    proc_exit: (code) => {
      throw new Error(`WebAssembly module exited with code ${code}`);
    },
    random_get: (bufferPointer, bufferLength) => {
      const bytes = getBytes();
      if (!bytes) {
        return 8;
      }

      const slice = bytes.subarray(bufferPointer, bufferPointer + bufferLength);
      if (globalThis.crypto?.getRandomValues) {
        globalThis.crypto.getRandomValues(slice);
        return 0;
      }

      for (let index = 0; index < slice.length; index += 1) {
        slice[index] = Math.floor(Math.random() * 256);
      }

      return 0;
    },
  };

  return {
    imports: {
      env: {
        emscripten_notify_memory_growth: () => {},
      },
      wasi_snapshot_preview1: wasi,
    },
    bindExports(wasmExports) {
      memoryRef = wasmExports.memory ?? wasmExports._memory ?? null;
    },
  };
}

export async function loadWasm(url, extraImports = {}) {
  const runtime = createStandaloneImports();
  const imports = mergeImports(runtime.imports, extraImports);

  let instance;
  if (WebAssembly.instantiateStreaming) {
    try {
      ({ instance } = await WebAssembly.instantiateStreaming(fetch(url), imports));
    } catch (error) {
      console.warn("instantiateStreaming failed, falling back to ArrayBuffer", error);
    }
  }

  if (!instance) {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Failed to fetch ${url}: HTTP ${response.status}`);
    }

    const buffer = await response.arrayBuffer();
    ({ instance } = await WebAssembly.instantiate(buffer, imports));
  }

  runtime.bindExports(instance.exports);
  return instance.exports;
}
