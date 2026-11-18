/*
 * Browser-side Swan WASM64 entry point.
 *
 * Browsers execute the portable WebAssembly module directly. SIC's native
 * host-compilation path is deliberately not involved in this environment.
 */
export async function runSwanWasm(url, imports = {}) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Unable to load Swan module: HTTP ${response.status}`);
  }

  let result;
  try {
    result = await WebAssembly.instantiateStreaming(response, imports);
  } catch (error) {
    // Some development servers serve .wasm with an incorrect MIME type.
    const fallback = await fetch(url);
    if (!fallback.ok) throw error;
    result = await WebAssembly.instantiate(await fallback.arrayBuffer(), imports);
  }

  const main = result.instance.exports.main;
  if (typeof main !== "function") {
    throw new Error("Swan WASM module does not export main()");
  }
  return main();
}
