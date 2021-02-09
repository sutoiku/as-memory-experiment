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
  const script = fs.readFileSync(path.join(__dirname, "tests.as"), "utf-8");

  const { binary } = asc.compileString(script, {
    optimize: 2,
    exportRuntime: true,
    importMemory: true,
    memoryBase: 4096,
  });

  console.timeEnd("compileString");

  const stdMem = new WebAssembly.Memory({
    initial: 100,
    maximum: 100,
  });

  const customMem = wamem.createMemory();

  await runFromMemory(binary, stdMem);
  await runFromMemory(binary, customMem);
}

async function runFromMemory(binary, memory) {
  console.log("----------------------------");

  const bufferSize = memory.buffer.byteLength;
  console.log("bufferSize", bufferSize);

  wamem.printArrayBufferBackingStoreFlags(memory.buffer);

  console.time("instantiate");
  const module = await loader.instantiate(binary, { env: { memory } });
  console.timeEnd("instantiate");

  const { testRead, testWrite, __getArrayView, __pin } = module.exports;
  testWrite(10, 42);
  console.log(testRead(10));

  try {
    testWrite(bufferSize + 1, 66);
  } catch (err) {
    console.error("CAUGHT", err);
  }

  console.log("----------------------------");
}
