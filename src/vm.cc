#include <iostream>
#include <node.h>
#include <v8.h>
#include <sstream>
#include <set>

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


  constexpr std::size_t VM_RESERVATION_SIZE = kFullGuardSize;
  // if we use the whole 4Gb: RuntimeError: WebAssembly.instantiate(): data segment is out of bounds
  // constexpr std::size_t VM_ALLOCATABLE_SIZE = kWasmPageSize * (kV8MaxWasmMemoryPages - 1);
  constexpr std::size_t VM_ALLOCATABLE_SIZE = kWasmPageSize * 16385; // AS fails on first heap allocation if we have more space ?!?
  constexpr std::ptrdiff_t VM_BASE_OFFSET = kNegativeGuardSize;

  static uintptr_t as_ptr(void *ptr) {
    return reinterpret_cast<uintptr_t>(ptr);
  }

  static void *as_ptr(uintptr_t ptr) {
    return reinterpret_cast<void *>(ptr);
  }

  static uintptr_t vm_allocate(uintptr_t address, std::size_t size, int prot) {
    return as_ptr(oscalls::mmap(as_ptr(address), size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  }

  static void vm_deallocate(uintptr_t address, std::size_t size) {
    oscalls::munmap(as_ptr(address), size);
  }
  
  struct region : public memory {
    region() {
      // TODO: test hugepages
      _base = vm_allocate(0, VM_RESERVATION_SIZE, PROT_NONE);
      _data = _base + VM_BASE_OFFSET;

      std::cout << "vm data: " << as_ptr(_data) << std::endl;
      std::cout << "vm base end: " << as_ptr(_data + VM_ALLOCATABLE_SIZE) << std::endl;
    }

    virtual ~region() {
      vm_deallocate(_base, VM_RESERVATION_SIZE); // TODO: verify that it does unmap inner mappings
    }

    virtual void *data() const override {
      return as_ptr(_data);
    }

    virtual std::size_t size() const override {
      return VM_ALLOCATABLE_SIZE;
    }

  private:
    uintptr_t _data;
    uintptr_t _base;
  };

  std::shared_ptr<memory> create_vm() {
    return std::make_shared<region>();
  }

  struct cow_memory;

  // TODO: use map to better access if we want to keep it
  static std::set<cow_memory *> cow_registry;

  // memory with COW to log memory accesses
  struct cow_memory : public memory {
    cow_memory() {
      // TODO: test hugepages
      _base = vm_allocate(0, VM_RESERVATION_SIZE, PROT_NONE);
      _data = _base + VM_BASE_OFFSET;

      std::cout << "vm data: " << as_ptr(_data) << std::endl;
      std::cout << "vm base end: " << as_ptr(_data + VM_ALLOCATABLE_SIZE) << std::endl;

      cow_registry.emplace(this);
    }

    virtual ~cow_memory() {
      vm_deallocate(_base, VM_RESERVATION_SIZE); // TODO: verify that it does unmap inner mappings
      cow_registry.erase(this);
    }

    virtual void *data() const override {
      return as_ptr(_data);
    }

    virtual std::size_t size() const override {
      return VM_ALLOCATABLE_SIZE;
    }

    bool try_handle_cow(uintptr_t fault_data_address) {
      if (fault_data_address >= _data && fault_data_address < _data + VM_ALLOCATABLE_SIZE) {
        const size_t page_size = 4096;
        const size_t mask = page_size - 1;
        const auto fault_base_address = fault_data_address & ~mask;
        std::cout << "handling cow at " << as_ptr(fault_data_address) << " base = " << as_ptr(fault_base_address) << std::endl;

        vm_allocate(fault_base_address, page_size, PROT_READ | PROT_WRITE);
        return true;
      }

      return false;
    }

  private:
    uintptr_t _data;
    uintptr_t _base;
  };

  bool handle_cow(uintptr_t fault_data_address) {
    for(auto cow : cow_registry) {
      if(cow->try_handle_cow(fault_data_address)) {
        return true;
      }
    }

    return false;
  }

  std::shared_ptr<memory> create_cow() {
    return std::make_shared<cow_memory>();
  }
}
