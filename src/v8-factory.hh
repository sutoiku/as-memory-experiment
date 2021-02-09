#pragma once

namespace experiment {
  v8::Local<v8::Object> create_v8_wa_memory(v8::Isolate* isolate, std::shared_ptr<memory> native_memory);
  void print_array_buffer_backing_store_flags(v8::Local<v8::ArrayBuffer> buffer);
}
