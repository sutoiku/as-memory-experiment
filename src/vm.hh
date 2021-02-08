#pragma once

namespace experiment {

  struct memory {
    memory() = default;
    virtual ~memory() = default;

    virtual void *data() const = 0;
    virtual std::size_t size() const = 0;
  };

  std::shared_ptr<memory> create_vm(std::size_t size);
}
