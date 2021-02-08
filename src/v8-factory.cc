#include <node.h>
#include <v8.h>

#include "vm.hh"
#include "v8-factory.hh"

// given newsgroup, Handle should be replaced by Local
// this are c++ internal headers
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"

// definition of BackingStore v8 structure, to be able to change their flags
namespace v8_structure_mapping {

  class SharedWasmMemoryData;

  struct BackingStore {

    // https://github.com/v8/v8/blob/dc712da548c7fb433caed56af9a021d964952728/src/objects/backing-store.h#L162
    void* buffer_start_;
    std::atomic<size_t> byte_length_;
    size_t byte_capacity_;

    struct DeleterInfo {
      v8::BackingStore::DeleterCallback callback;
      void* data;
    };

    union TypeSpecificData {
      TypeSpecificData() : v8_api_array_buffer_allocator(nullptr) {}
      ~TypeSpecificData() {}

      // If this backing store was allocated through the ArrayBufferAllocator API,
      // this is a direct pointer to the API object for freeing the backing
      // store.
      v8::ArrayBuffer::Allocator* v8_api_array_buffer_allocator;

      // Holds a shared_ptr to the ArrayBuffer::Allocator instance, if requested
      // so by the embedder through setting
      // Isolate::CreateParams::array_buffer_allocator_shared.
      std::shared_ptr<v8::ArrayBuffer::Allocator>
          v8_api_array_buffer_allocator_shared;

      // For shared Wasm memories, this is a list of all the attached memory
      // objects, which is needed to grow shared backing stores.
      SharedWasmMemoryData* shared_wasm_memory_data;

      // Custom deleter for the backing stores that wrap memory blocks that are
      // allocated with a custom allocator.
      DeleterInfo deleter;
    } type_specific_data_;

    bool is_shared_ : 1;
    bool is_wasm_memory_ : 1;
    bool holds_shared_ptr_to_allocator_ : 1;
    bool free_on_destruct_ : 1;
    bool has_guard_regions_ : 1;
    bool globally_registered_ : 1;
    bool custom_deleter_ : 1;
    bool empty_deleter_ : 1;
  };
}

// v8 internal utils to translate Local <-> Handle (originally in v8 c api implementation)
namespace v8_internal_utils {

  template<typename To, typename From>
  v8::internal::Handle<To> OpenHandle(          
      const From* that, bool allow_empty_handle = false) {               
/*
    DCHECK(allow_empty_handle || that != nullptr);                   
    DCHECK(that == nullptr ||                                        
           v8::internal::Object(                                     
               *reinterpret_cast<const v8::internal::Address*>(that))
               .Is##To());                                           
*/
    return v8::internal::Handle<To>(                   
        reinterpret_cast<v8::internal::Address*>(                    
            const_cast<From*>(that)));                           
  }

    template<typename T>
    struct FakeLocal {
      T* ptr;
    };

  // named Convert in v8 api source
  template<typename To, typename From>
  inline v8::Local<To> ToLocal(v8::internal::Handle<From> obj) {
  // DCHECK(obj.is_null() || (obj->IsSmi() || !obj->IsTheHole()));

    // local only contains ptr
    // return v8::Local<To>(reinterpret_cast<To*>(obj.location()));

    auto local = v8::Local<To>();
    auto fakeLocal = reinterpret_cast<FakeLocal<To>*>(&local);
    fakeLocal->ptr = reinterpret_cast<To*>(obj.location());
    return local;
  }
}

// definition of v8 internal structures, only to match WasmMemoryObject::New exported symbol
namespace v8 {
  namespace internal {

    class Object {
    };
    
    class JSArrayBuffer : public Object {
    };

    class Isolate;

    class WasmMemoryObject {
    public:
      // static v8::Handle<WasmMemoryObject> New(v8::Isolate* isolate, v8::MaybeHandle<v8::JSArrayBuffer> buffer, uint32_t maximum);
      static v8::internal::Handle<WasmMemoryObject> New(v8::internal::Isolate* isolate, v8::internal::MaybeHandle<v8::internal::JSArrayBuffer> buffer, uint32_t maximum);
    };

  }
}

namespace {

  template<typename T>
  struct shared_ptr_holder {
    std::shared_ptr<T> ptr;
  };

  template<typename T>
  static void backing_store_deleter(void* buffer, size_t length, void* info) {
    shared_ptr_holder<T> *holder = reinterpret_cast<shared_ptr_holder<T> *>(info);
    delete holder;
  }

}

namespace experiment {

  v8::Local<v8::Object> create_v8_wa_memory(v8::Isolate* isolate, std::shared_ptr<memory> native_memory) {
    auto holder = new shared_ptr_holder<memory>();
    holder->ptr = native_memory;

    auto backing_store = v8::ArrayBuffer::NewBackingStore(
        native_memory->data(), native_memory->size(),
        backing_store_deleter<memory>, holder);
    
    // TODO: if it fails, we MUST delete holder

    auto internal_store = reinterpret_cast<v8_structure_mapping::BackingStore *>(backing_store.get());
    internal_store->has_guard_regions_ = true; // TODO: this is NOT true, for now, we MUST implement guard regions

    auto buffer = v8::ArrayBuffer::New(isolate, std::move(backing_store));

    // use v8 internal API to craft WebAssembly.Memory object
    auto handle = v8_internal_utils::OpenHandle<v8::internal::JSArrayBuffer>(*buffer);
    auto internal_isolate = reinterpret_cast<v8::internal::Isolate *>(isolate);
    auto new_memory = v8::internal::WasmMemoryObject::New(internal_isolate, handle, 10);
    return v8_internal_utils::ToLocal<v8::Object>(new_memory);
  }

}