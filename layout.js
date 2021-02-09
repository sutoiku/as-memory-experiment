"use strict";

const path = require("path");
const fs = require("fs");
const asc = require("assemblyscript/cli/asc");
const loader = require("assemblyscript/lib/loader");
const wamem = require("./build/Release/wamem");

// https://www.assemblyscript.org/memory.html#internals

main();

async function main() {
  await asc.ready;

  wamem.setupTrap();

  console.time("compileString");
  const script = fs.readFileSync(path.join(__dirname, "layout.as"), "utf-8");
  console.timeEnd("compileString");

  const { binary } = asc.compileString(script, {
    optimize: 2,
    exportRuntime: true,
    importMemory: true,
    memoryBase: 65536, // at COMPILE time
  });

  const memory = wamem.createCowMemory();

  const bufferSize = memory.buffer.byteLength;
  console.log("bufferSize", bufferSize);

  console.time("instantiate");
  const module = await loader.instantiate(binary, { env: { memory } });
  console.timeEnd("instantiate");

  const { staticString, newArray, __heap_base } = module.exports;
  console.log(Object.keys(module.exports));
  console.log("staticString", staticString.value);
  console.log("newArray", newArray());
  console.log("__heap_base", __heap_base);
}
