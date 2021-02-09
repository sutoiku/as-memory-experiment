#include <node.h>
#include <v8.h>

#include "vm.hh"
#include "trap.hh"
#include "v8-factory.hh"

namespace experiment {

  void createMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    // auto context = isolate->GetCurrentContext();

    // auto buffer_size = args[0]->Uint32Value(context).ToChecked();

    auto memory = create_vm();
    auto wamem = create_v8_wa_memory(isolate, memory);
    args.GetReturnValue().Set(wamem);
  }

  void setupTrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    setup_trap();
  }

  void printArrayBufferBackingStoreFlags(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();
    auto array_buffer = v8::Local<v8::ArrayBuffer>::Cast(args[0]->ToObject(context).ToLocalChecked());
    print_array_buffer_backing_store_flags(array_buffer);
  }

  void init(v8::Local<v8::Object> exports) {
    NODE_SET_METHOD(exports, "createMemory", createMemory);
    NODE_SET_METHOD(exports, "setupTrap", setupTrap);
    NODE_SET_METHOD(exports, "printArrayBufferBackingStoreFlags", printArrayBufferBackingStoreFlags);
  }

  NODE_MODULE(wamem, init)

}
