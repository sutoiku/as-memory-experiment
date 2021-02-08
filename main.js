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

  /*const memory = new WebAssembly.Memory({
    initial: 100,
    maximum: 100,
  });*/

  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/Memory
  // Note: A WebAssembly page has a constant size of 65,536 bytes, i.e., 64KiB.
  const bufferSize = 65536 * 10;
  const memory = wamem.createMemory(bufferSize);

  console.time("instantiate");
  const module = await loader.instantiate(binary, { env: { memory } });
  console.timeEnd("instantiate");

  const { testRead, testWrite, __getArrayView, __pin } = module.exports;
  testWrite(10, 42);
  console.log(testRead(10));

  testWrite(65536 * 10 + 10, 66);
}
