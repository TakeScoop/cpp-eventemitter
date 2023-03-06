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

class StringConstructable : public Constructable {
public:
  explicit StringConstructable(const std::string &value);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  std::string m_value;
};

class UndefinedConstructable : public Constructable {
public:
  explicit UndefinedConstructable();

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;
};

class FalseConstructable : public Constructable {
public:
  explicit FalseConstructable();

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;
};

class TrueConstructable : public Constructable {
public:
  explicit TrueConstructable();

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;
};

class BooleanConstructable : public Constructable {
public:
  explicit BooleanConstructable(bool &value);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  bool m_value;
};

class NullConstructable : public Constructable {
public:
  explicit NullConstructable();

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;
};

class TypeErrorConstructable : public Constructable {
public:
  explicit TypeErrorConstructable(const std::string &value);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  std::string m_value;
};

class IntNumberConstructable : public Constructable {
public:
  explicit IntNumberConstructable(int32_t &value);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  int32_t m_value;
};

class DoubleNumberConstructable : public Constructable {
public:
  explicit DoubleNumberConstructable(double &value);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  double m_value;
};

using ObjectValues =
    std::vector<std::pair<std::string, std::shared_ptr<Constructable>>>;

class ObjectConstructable : public Constructable {
public:
  explicit ObjectConstructable(ObjectValues values);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  ObjectValues m_values;
};

using ArrayValues = std::vector<std::shared_ptr<Constructable>>;

class ArrayConstructable : public Constructable {
public:
  explicit ArrayConstructable(ArrayValues values);

  virtual v8::Local<v8::Value> construct(Nan::HandleScope &scope,
                                         v8::Isolate *isolate) const override;

private:
  ArrayValues m_values;
};

#endif