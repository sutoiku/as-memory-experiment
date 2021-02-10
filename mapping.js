"use strict";

const path = require("path");
const fs = require("fs");
const asc = require("assemblyscript/cli/asc");
const loader = require("assemblyscript/lib/loader");
const wamem = require("./build/Release/wamem");

// https://www.assemblyscript.org/memory.html#internals

main();

const FILE = "/tmp/as-test";

async function main() {
  await asc.ready;

  wamem.setupTrap();

  console.time("compileString");
  const script = fs.readFileSync(path.join(__dirname, "mapping.as"), "utf-8");
  console.timeEnd("compileString");

  const reservation = 512 * 1024 * 1024;

  const { binary } = asc.compileString(script, {
    optimize: 2,
    exportRuntime: true,
    importMemory: true,
    memoryBase: reservation, // at COMPILE time
  });

  const memory = wamem.createMemory(reservation);

  const bufferSize = memory.buffer.byteLength;

  console.time("instantiate");
  const module = await loader.instantiate(binary, { env: { memory } });
  console.timeEnd("instantiate");

  const { testRead, testWrite } = module.exports;

  {
    const content = Buffer.from([0x10, 0x20, 0x30, 0x40, 0x50, 0x60]);
    fs.writeFileSync(FILE, content);
    const id = wamem.vmMapFile(FILE, 0x1000, content.length, false);
    console.log("should be 0x20", testRead(0x1001));

    try {
      testWrite(0x1000, 42);
    } catch (err) {
      console.log("should fail", err);
    }

    wamem.vmUnmapFile(id);
  }

  {
    const content = Buffer.from([0, 0, 0, 0, 0, 0]);
    fs.writeFileSync(FILE, content);
    const id = wamem.vmMapFile(FILE, 0x1000, content.length, true);
    console.log("should be 0x0", testRead(0x1001));

    testWrite(0x1002, 42);

    wamem.vmUnmapFile(id);

    const newContent = fs.readFileSync(FILE);
    console.log("should be 0, 0, 42, 0, 0, 0", newContent);
  }
}
