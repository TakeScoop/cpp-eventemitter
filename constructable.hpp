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

#pragma once
#ifndef _NODE_EVENT_EVENTEMITTER_CONSTRUCTABLE_H
#define _NODE_EVENT_EVENTEMITTER_CONSTRUCTABLE_H

/* abstract */ class Constructable {
public:
  explicit Constructable(){

  };

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const = 0;
};

using EventValue = std::shared_ptr<Constructable> const &;

using ObjectValues = std::vector<std::pair<std::string, Constructable>>;

class StringConstructable : public Constructable {
public:
  explicit StringConstructable(std::string value)
      : Constructable(), m_value{std::move(value)} {};

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override {

    return Nan::New<v8::String>(m_value).ToLocalChecked();
  };

private:
  std::string m_value;
};

/* class ObjectConstructable : public Constructable {
public:
  explicit ObjectConstructable(ObjectValues values)
      : Constructable(), m_values{std::move(values)}, {};

  virtual v8::Local<v8::Array> construct(Nan::HandleScope &scope,
                                               v8::Isolate *isolate) const
override {

    v8::Local<v8::Object> result = v8::Object::New(isolate);

    for (const auto &[key, value] : m_values) {
      Nan::Local<v8::String> keyValue =
          Nan::New<v8::String>(key).ToLocalChecked();
      result.Set(isolate.GetCurrentContext(), keyValue, value.construct(scope,
isolate));
    }

    return result;
  };

private:
  ObjectValues m_values;
}; */

#endif