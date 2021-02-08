#include <node.h>
#include <v8.h>

#include "vm.hh"

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

  std::shared_ptr<memory> create_vm(std::size_t size) {
    return std::make_shared<simple_memory>(size);
  }
}
