#include <node.h>
#include <v8.h>

#include "vm.hh"
#include "trap.hh"
#include "v8-factory.hh"

namespace experiment {

  void createMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();

    auto reservation_size = args[0]->Uint32Value(context).ToChecked();

    auto memory = create_vm(reservation_size);
    auto wamem = create_v8_wa_memory(isolate, memory);
    args.GetReturnValue().Set(wamem);
  }

  void vmMapFile(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();

    auto path = std::string(*::v8::String::Utf8Value(isolate, args[0]->ToString(context).ToLocalChecked()));
    auto offset = args[1]->Uint32Value(context).ToChecked();
    auto size = args[2]->Uint32Value(context).ToChecked();
    auto writable = args[3]->BooleanValue(isolate);

    auto id = vm_map_file(path, offset, size, writable);
    args.GetReturnValue().Set(id);
  }

  void vmUnmapFile(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();

    auto id = args[0]->Int32Value(context).ToChecked();

    vm_unmap_file(id);
  }

  void createCowMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    // auto context = isolate->GetCurrentContext();

    // auto buffer_size = args[0]->Uint32Value(context).ToChecked();

    auto memory = create_cow();
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
    NODE_SET_METHOD(exports, "createCowMemory", createCowMemory);
    NODE_SET_METHOD(exports, "setupTrap", setupTrap);
    NODE_SET_METHOD(exports, "printArrayBufferBackingStoreFlags", printArrayBufferBackingStoreFlags);
  }

  NODE_MODULE(wamem, init)

}
