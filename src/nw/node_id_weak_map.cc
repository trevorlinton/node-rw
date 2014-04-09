// Copyright (c) 2012 Intel Corp
// Copyright (c) 2012 The Chromium Authors
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell co
// pies of the Software, and to permit persons to whom the Software is furnished
//  to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in al
// l copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM
// PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNES
// S FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
//  OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WH
// ETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
//  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"

#include "node_object_wrap.h"
#include "util.h"
#include "util-inl.h"

#include <map>

namespace nw {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

// Key is int type and Value is weak pointer
class IDWeakMap : public node::ObjectWrap {
 public:
  static void Init();
  static v8::Local<v8::Function> GetContructor();
  static void AttachBindings(v8::Handle<v8::Object> obj);

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Set(const FunctionCallbackInfo<Value>& args);
  static void Get(const FunctionCallbackInfo<Value>& args);
  static void Has(const FunctionCallbackInfo<Value>& args);
  static void Delete(const FunctionCallbackInfo<Value>& args);
  static void AllocateId(const FunctionCallbackInfo<Value>& args);
  static void Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context);

 private:
  explicit IDWeakMap();
  virtual ~IDWeakMap();

  void Erase(int key);

  static void WeakCallback(v8::Isolate* isolate, v8::Persistent<v8::Value>* value, IDWeakMap *data);

  static v8::Persistent<v8::Function> constructor_;
  typedef v8::Persistent<v8::Value, v8::CopyablePersistentTraits<v8::Value> >
      CopyableValue;
  typedef std::map< int, CopyableValue > CopyableValueMap;
  CopyableValueMap map_;
};

v8::Persistent<v8::Function> IDWeakMap::constructor_;

// static
void IDWeakMap::Init() {
  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
  tpl->SetClassName(v8::String::NewSymbol("IDWeakMap"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("set"),
      v8::FunctionTemplate::New(Set)->GetFunction());
  tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("get"),
      v8::FunctionTemplate::New(Get)->GetFunction());
  tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("has"),
      v8::FunctionTemplate::New(Has)->GetFunction());
  tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("delete"),
      v8::FunctionTemplate::New(Delete)->GetFunction());
  tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("allocateId"),
      v8::FunctionTemplate::New(AllocateId)->GetFunction());

  constructor_.Reset(node::node_isolate, tpl->GetFunction());
}

// static
v8::Local<v8::Function> IDWeakMap::GetContructor() {
  if (constructor_.IsEmpty())
    Init();

  return node::StrongPersistentToLocal(constructor_);
}

// static
void IDWeakMap::AttachBindings(v8::Handle<v8::Object> obj) {
  v8::HandleScope scope;

  obj->Set(v8::String::New("IDWeakMap"), GetContructor());
}

// static
void IDWeakMap::New(const FunctionCallbackInfo<Value>& args) {
  IDWeakMap* obj = new IDWeakMap();
  obj->Wrap(args.This());
  args.GetReturnValue().Set(args.This());
}

// static
void IDWeakMap::Set(const FunctionCallbackInfo<Value>& args) {
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsObject()) {
    args.GetIsolate()->ThrowException(v8::Exception::Error(
          v8::String::New("Invalid arguments")));
    return;
  }

  IDWeakMap* obj = ObjectWrap::Unwrap<IDWeakMap>(args.This());

  int key = args[0]->IntegerValue();

  if (obj->map_.find(key) != obj->map_.end()) {
    args.GetIsolate()->ThrowException(v8::Exception::Error(
          v8::String::New("Element already exists")));
    return;
  }

  v8::Persistent<v8::Value> value(node::node_isolate, args[1]);
  value->ToObject()->SetHiddenValue(
      v8::String::New("IDWeakMapKey"), v8::Integer::New(key));

  obj->map_[key] = value;
  value.MakeWeak(obj, WeakCallback);
  args.GetReturnValue().Set(v8::Undefined());
}

// static
void IDWeakMap::Get(const FunctionCallbackInfo<Value>& args) {
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    args.GetIsolate()->ThrowException(v8::Exception::Error(
          v8::String::New("Invalid arguments")));
    return;
  }

  IDWeakMap* obj = ObjectWrap::Unwrap<IDWeakMap>(args.This());

  int key = args[0]->IntegerValue();
  CopyableValueMap::iterator it = obj->map_.find(key);
  if (it == obj->map_.end()) {
    args.GetReturnValue().Set(v8::Null());
    return;
  }
  CopyableValue value = it->second;
  args.GetReturnValue().Set(v8::Persistent<v8::Value>(node::node_isolate, value));
}

// static
void IDWeakMap::Has(const FunctionCallbackInfo<Value>& args) {
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    args.GetIsolate()->ThrowException(v8::Exception::Error(
          v8::String::New("Invalid arguments")));
    return;
  }

  IDWeakMap* obj = ObjectWrap::Unwrap<IDWeakMap>(args.This());

  int key = args[0]->IntegerValue();
  args.GetReturnValue().Set(v8::Boolean::New(!obj->map_[key].IsEmpty()));
}

// static
void IDWeakMap::Delete(const FunctionCallbackInfo<Value>& args) {
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    args.GetIsolate()->ThrowException(v8::Exception::Error(
          v8::String::New("Invalid arguments")));
    return;
  }

  IDWeakMap* obj = ObjectWrap::Unwrap<IDWeakMap>(args.This());

  int key = args[0]->IntegerValue();
  obj->Erase(key);
  args.GetReturnValue().Set(v8::Undefined());
}

// static
void IDWeakMap::AllocateId(const FunctionCallbackInfo<Value>& args) {
  static int next_object_id = 1;
  args.GetReturnValue().Set(v8::Integer::New(next_object_id++));
}

IDWeakMap::IDWeakMap() {
}

IDWeakMap::~IDWeakMap() {
}

void IDWeakMap::Erase(int key) {
  CopyableValue& value = map_[key];
  value.ClearWeak(v8::Isolate::GetCurrent());
  value.Dispose();
  value.Clear();
  map_.erase(key);
}

// static
void IDWeakMap::WeakCallback(v8::Isolate* isolate, v8::Persistent<v8::Value>* value, IDWeakMap *data) {
  v8::HandleScope scope;

  IDWeakMap* obj = static_cast<IDWeakMap*>(data);
  int key = (*value)->ToObject()->GetHiddenValue(
      v8::String::New("IDWeakMapKey"))->IntegerValue();
  obj->Erase(key);
}

//static
void IDWeakMap::Initialize(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  // Environment* env = Environment::GetCurrent(context);
  IDWeakMap::AttachBindings(target);
}

}  // namespace nw

NODE_MODULE_CONTEXT_AWARE(node_id_weak_map, nw::IDWeakMap::Initialize)
