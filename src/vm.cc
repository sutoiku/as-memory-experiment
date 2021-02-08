#include <iostream>
#include <node.h>
#include <v8.h>
#include <sstream>

#include "vm.hh"
#include "ryu-os-calls.hh"

namespace experiment {

  struct simple_memory : public memory {
    simple_memory(std::size_t psize)
     : _buffer(std::make_unique<char[]>(psize))
     , _size(psize) {
    }

    virtual ~simple_memory() {
    }

    virtual void *data() const override {
      return _buffer.get();
    }

    virtual std::size_t size() const override {
      return _size;
    }

  private:
    std::unique_ptr<char[]> _buffer;
    std::size_t _size;
  };

  // v8 defs
  constexpr size_t kWasmPageSize = 0x10000; // 64kb
  constexpr size_t kV8MaxWasmMemoryPages = 65536;  // = 4 GiB
  constexpr uint64_t GB = 1024 * 1024 * 1024;
  constexpr uint64_t kNegativeGuardSize = uint64_t{2} * GB;
  constexpr uint64_t kFullGuardSize = uint64_t{10} * GB;
  // ---

  // if we use the whole 4Gb: RuntimeError: WebAssembly.instantiate(): data segment is out of bounds
  constexpr std::size_t WASM_BUFFER_SIZE = kWasmPageSize * (kV8MaxWasmMemoryPages - 1);

  struct region : public memory {
    // like v8 region guards
    region() {
      _base = as_ptr(oscalls::mmap(nullptr, kFullGuardSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)); // TODO: test hugepages
      _data = as_ptr(oscalls::mmap(as_ptr(_base + kNegativeGuardSize), WASM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)); // TODO: test hugepages

      std::cout << "vm base: " << as_ptr(_base) << std::endl;
      std::cout << "vm base end: " << as_ptr(_base + kFullGuardSize) << std::endl;
      std::cout << "vm data: " << as_ptr(_data) << std::endl;
      std::cout << "vm base end: " << as_ptr(_data + WASM_BUFFER_SIZE) << std::endl;
    }

    virtual ~region() {
    }

    virtual void *data() const override {
      return as_ptr(_data);
    }

    virtual std::size_t size() const override {
      return WASM_BUFFER_SIZE;
    }

  private:

    uintptr_t as_ptr(void *ptr) const {
      return reinterpret_cast<uintptr_t>(ptr);
    }

    void *as_ptr(uintptr_t ptr) const {
      return reinterpret_cast<void *>(ptr);
    }

    uintptr_t _data;
    uintptr_t _base;
  };

  std::shared_ptr<memory> create_vm() {
    return std::make_shared<region>();
  }
}
