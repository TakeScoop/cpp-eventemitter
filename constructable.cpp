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

/* abstract */ class Constructable {
public:
  explicit Constructable(){

  };

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const = 0;
};

class StringConstructable : public Constructable {
public:
  explicit StringConstructable(const std::string &value)
      : Constructable(), m_value{std::move(value)} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::New<v8::String>(m_value).ToLocalChecked();
  };

private:
  std::string m_value;
};

class UndefinedConstructable : public Constructable {
public:
  explicit UndefinedConstructable() : Constructable(){};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::Undefined();
  };
};

class FalseConstructable : public Constructable {
public:
  explicit FalseConstructable() : Constructable(){};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::False();
  };
};

class TrueConstructable : public Constructable {
public:
  explicit TrueConstructable() : Constructable(){};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::True();
  };
};

class NullConstructable : public Constructable {
public:
  explicit NullConstructable() : Constructable(){};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::Null();
  };
};

class TypeErrorConstructable : public Constructable {
public:
  explicit TypeErrorConstructable(const std::string &value)
      : Constructable(), m_value{std::move(value)} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::TypeError(m_value.c_str());
  };

private:
  std::string m_value;
};

class IntNumberConstructable : public Constructable {
public:
  explicit IntNumberConstructable(int32_t &value)
      : Constructable(), m_value{value} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {
    return Nan::New<v8::Int32>(m_value);
  };

private:
  int32_t m_value;
};

class DoubleNumberConstructable : public Constructable {
public:
  explicit DoubleNumberConstructable(double &value)
      : Constructable(), m_value{value} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {
    return Nan::New<v8::Number>(m_value);
  };

private:
  double m_value;
};

class ObjectConstructable : public Constructable {
public:
  explicit ObjectConstructable(ObjectValues values)
      : Constructable(), m_values{std::move(values)} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    v8::Local<v8::Object> result = v8::Object::New(isolate);

    for (const auto &[key, value] : m_values) {
      v8::Local<v8::String> keyValue =
          Nan::New<v8::String>(key).ToLocalChecked();
      result
          ->Set(isolate->GetCurrentContext(), keyValue,
                value->construct(scope, isolate))
          .Check();
    }

    return v8::Local<v8::Value>::Cast(result);
  };

private:
  ObjectValues m_values;
};

using ArrayValues = std::vector<std::shared_ptr<Constructable>>;

class ArrayConstructable : public Constructable {
public:
  explicit ArrayConstructable(ArrayValues values)
      : Constructable(), m_values{std::move(values)} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    v8::Local<v8::Array> result = Nan::New<v8::Array>(m_values.size());

    size_t i = 0;
    for_each(m_values.begin(), m_values.end(),
             [&](std::shared_ptr<Constructable> value) {
               Nan::Set(result, i, value->construct(scope, isolate));
               ++i;
             });

    return v8::Local<v8::Value>::Cast(result);
  };

private:
  ArrayValues m_values;
};