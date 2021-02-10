#pragma once

namespace experiment {

  struct memory {
    memory() = default;
    virtual ~memory() = default;

    virtual void *data() const = 0;
    virtual std::size_t size() const = 0;
  };

  std::shared_ptr<memory> create_cow();
  bool handle_cow(uintptr_t fault_data_address);

  std::shared_ptr<memory> create_vm(size_t reservation_size);
  int vm_map_file(const std::string &path, uintptr_t offset, size_t size, bool writable);
  void vm_unmap_file(int id);
}
