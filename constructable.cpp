/*
 * Copyright 2023 Totto16
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <memory>
#include <nan.h>
#include <string>
#include <utility>
#include <vector>

#include "constructable.hpp"

StringConstructable::StringConstructable(const std::string &value)
    : Constructable(), m_value{std::move(value)} {};

v8::Local<v8::Value>
StringConstructable::construct(Nan::HandleScope &scope,
                               v8::Isolate *isolate) const {

  return Nan::New<v8::String>(m_value).ToLocalChecked();
};

UndefinedConstructable::UndefinedConstructable() : Constructable(){};

v8::Local<v8::Value>
UndefinedConstructable::construct(Nan::HandleScope &scope,
                                  v8::Isolate *isolate) const {

  return Nan::Undefined();
};

FalseConstructable::FalseConstructable() : Constructable(){};

v8::Local<v8::Value> FalseConstructable::construct(Nan::HandleScope &scope,
                                                   v8::Isolate *isolate) const {

  return Nan::False();
};

TrueConstructable::TrueConstructable() : Constructable(){};

v8::Local<v8::Value> TrueConstructable::construct(Nan::HandleScope &scope,
                                                  v8::Isolate *isolate) const {

  return Nan::True();
};

BooleanConstructable::BooleanConstructable(bool &value)
    : Constructable(), m_value{value} {};

v8::Local<v8::Value>
BooleanConstructable::construct(Nan::HandleScope &scope,
                                v8::Isolate *isolate) const {
  if (m_value) {
    return Nan::True();
  }

  return Nan::False();
};

NullConstructable::NullConstructable() : Constructable(){};

v8::Local<v8::Value> NullConstructable::construct(Nan::HandleScope &scope,
                                                  v8::Isolate *isolate) const {

  return Nan::Null();
};

TypeErrorConstructable::TypeErrorConstructable(const std::string &value)
    : Constructable(), m_value{std::move(value)} {};

v8::Local<v8::Value>
TypeErrorConstructable::construct(Nan::HandleScope &scope,
                                  v8::Isolate *isolate) const {

  return Nan::TypeError(m_value.c_str());
};

IntNumberConstructable::IntNumberConstructable(int32_t &value)
    : Constructable(), m_value{value} {};

v8::Local<v8::Value>
IntNumberConstructable::construct(Nan::HandleScope &scope,
                                  v8::Isolate *isolate) const {
  return Nan::New<v8::Int32>(m_value);
};

DoubleNumberConstructable::DoubleNumberConstructable(double &value)
    : Constructable(), m_value{value} {};

v8::Local<v8::Value>
DoubleNumberConstructable::construct(Nan::HandleScope &scope,
                                     v8::Isolate *isolate) const {
  return Nan::New<v8::Number>(m_value);
};

ObjectConstructable::ObjectConstructable(ObjectValues values)
    : Constructable(), m_values{std::move(values)} {};

v8::Local<v8::Value>
ObjectConstructable::construct(Nan::HandleScope &scope,
                               v8::Isolate *isolate) const {

  v8::Local<v8::Object> result = v8::Object::New(isolate);

  for (const auto &[key, value] : m_values) {
    v8::Local<v8::String> keyValue = Nan::New<v8::String>(key).ToLocalChecked();
    result
        ->Set(isolate->GetCurrentContext(), keyValue,
              value->construct(scope, isolate))
        .Check();
  }

  return v8::Local<v8::Value>::Cast(result);
};

using ArrayValues = std::vector<std::shared_ptr<Constructable>>;

ArrayConstructable::ArrayConstructable(ArrayValues values)
    : Constructable(), m_values{std::move(values)} {};

v8::Local<v8::Value> ArrayConstructable::construct(Nan::HandleScope &scope,
                                                   v8::Isolate *isolate) const {

  v8::Local<v8::Array> result = Nan::New<v8::Array>(m_values.size());

  size_t i = 0;
  for_each(m_values.begin(), m_values.end(),
           [&](std::shared_ptr<Constructable> value) {
             Nan::Set(result, i, value->construct(scope, isolate));
             ++i;
           });

  return v8::Local<v8::Value>::Cast(result);
};
