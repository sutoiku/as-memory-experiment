#include <iostream>
#include <node.h>
#include <v8.h>
#include <sstream>
#include <set>
#include <map>

#include "vm.hh"
#include "ryu-os-calls.hh"

namespace experiment {

  // ---------------------------------------------------------------------------
  // SIMPLE
  // ---------------------------------------------------------------------------

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

  static uintptr_t vm_allocate(uintptr_t address, std::size_t size, int prot, int fd = -1) {
    auto flags = 0;
    if (fd != -1) {
      flags |= MAP_SHARED;
    } else {
      flags |= MAP_PRIVATE | MAP_ANONYMOUS;
    }

    if (address != 0) {
      flags |= MAP_FIXED;
    }

    return as_ptr(oscalls::mmap(as_ptr(address), size, prot, flags, fd, 0));
  }

  static void vm_deallocate(uintptr_t address, std::size_t size) {
    oscalls::munmap(as_ptr(address), size);
  }

  // ---------------------------------------------------------------------------
  // COW
  // ---------------------------------------------------------------------------

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

        vm_allocate(fault_base_address, page_size, PROT_READ | PROT_WRITE);

        std::cout << "handling cow at " << as_ptr(fault_data_address) << " offset = " << as_ptr(fault_base_address - _data) << std::endl;
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

  // ---------------------------------------------------------------------------
  // REGION
  // ---------------------------------------------------------------------------

  struct mapping {
    mapping(int id, const std::string &path, uintptr_t address, size_t size, bool writable)
     : _address(address)
     , _size(size) {
      int fd = oscalls::open(path.c_str(), writable ? O_RDWR : O_RDONLY);

      int flags = PROT_READ;
      if(writable) {
        flags |= PROT_WRITE;
      }

      vm_allocate(_address, _size, flags, fd);

      close(fd);
    }

    ~mapping() {
      // erase the mapping with an inaccessible one
      // TODO: this need care when called from region dtor (because vm_deallocate already called?)
      // vm_deallocate(_address, _size);
      vm_allocate(_address, _size, PROT_NONE);
    }

    bool is_overlap(uintptr_t address, size_t size) const {
      // https://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap/325964#325964
      auto starta = address;
      auto enda = address + size;
      auto startb = _address;
      auto endb = _address + _size;
      return starta <= endb && startb <= enda;
    }

  private:
    uintptr_t _address;
    size_t _size;
  };
    
  struct region : public memory {
    region(size_t reservation_size)
     : _reservation_size(reservation_size), _mapping_id_counter(0) {
      // TODO: test hugepages
      _base = vm_allocate(0, VM_RESERVATION_SIZE, PROT_NONE);
      _data = _base + VM_BASE_OFFSET;

      vm_allocate(heap_base(), heap_size(), PROT_READ | PROT_WRITE);

      std::cout << "vm data: " << as_ptr(_data) << " -> " << as_ptr(_data + VM_ALLOCATABLE_SIZE) << std::endl;
      std::cout << "vm heap: " << as_ptr(heap_base()) << " -> " << as_ptr(heap_base() + heap_size()) << std::endl;
      std::cout << "vm mappable space: " << as_ptr(mappable_base()) << " -> " << as_ptr(mappable_base() + mappable_size()) << std::endl;
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

    int map_file(const std::string &path, uintptr_t offset, size_t size, bool writable) {
      auto address = absolute(offset);
      for(const auto & [id, mapping]: _mappings) {
        if(mapping->is_overlap(address, size)) {
          std::cout << "map_file overlap id " << id << std::endl;
          abort();
        }
      }

      auto id = ++_mapping_id_counter;
      _mappings.emplace(id, std::make_unique<mapping>(id, path, address, size, writable));
      return id;
    }

    void unmap_file(int id) {
      _mappings.erase(id);
    }

  private:
    uintptr_t heap_base() const {
      return _data + _reservation_size;
    }

    size_t heap_size() const {
      return VM_ALLOCATABLE_SIZE - _reservation_size;
    }

    uintptr_t mappable_base() const {
      return _data;
    }

    size_t mappable_size() const {
      return _reservation_size;
    }

    uintptr_t absolute(uintptr_t offset) {
      return _data + offset;
    }

    uintptr_t _base;
    uintptr_t _data;
    size_t _reservation_size;
    int _mapping_id_counter;
    std::map<int, std::unique_ptr<mapping>> _mappings;
  };

  // for now store it globally here as a crado
  static std::shared_ptr<region> global_region;

  std::shared_ptr<memory> create_vm(size_t reservation_size) {
    global_region = std::make_shared<region>(reservation_size);
    return global_region;
  }

  int vm_map_file(const std::string &path, uintptr_t offset, size_t size, bool writable) {
    return global_region->map_file(path, offset, size, writable);
  }

  void vm_unmap_file(int id) {
    return global_region->unmap_file(id);
  }
}
