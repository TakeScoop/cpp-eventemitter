#include <node.h>
#include <iostream>
#include <sstream>
#include <thread>

#include "../../eventemitter.hpp"

using namespace std;
using namespace NodeEvent;
using namespace Nan;
using namespace v8;

class TestWorker : public AsyncEventEmittingCWorker<16> {
 public:
    TestWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter, size_t n)
        : AsyncEventEmittingCWorker(callback, emitter), n_(n) {}

    virtual void ExecuteWithEmitter(eventemitter_fn emitter) override {
        for (int32_t i = 0; i < n_; ++i) {
            stringstream ss;
            ss << "Test" << i;
            while(!emitter("test", ss.str().c_str())){
                std::this_thread::yield();
            }
            while(!emitter("test2", ss.str().c_str())){
                std::this_thread::yield();
            }
            while(!emitter("test3", ss.str().c_str())){
                std::this_thread::yield();
            }
        }
    }

 private:
    int32_t n_;
};

class TestReentrantWorker : public AsyncEventEmittingReentrantCWorker<16> {
 public:
    TestReentrantWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter, size_t n)
        : AsyncEventEmittingReentrantCWorker(callback, emitter), n_(n) {}

    virtual void ExecuteWithEmitter(const ExecutionProgressSender* sender, eventemitter_fn_r emitter) override {
        for (int32_t i = 0; i < n_; ++i) {
            stringstream ss;
            ss << "Test" << i;
            while(!emitter((void*)sender, "test", ss.str().c_str())) {
                std::this_thread::yield();
            }
            while(!emitter((void*)sender, "test2", ss.str().c_str())) {
                std::this_thread::yield();
            }
            while(!emitter((void*)sender, "test3", ss.str().c_str())) {
                std::this_thread::yield();
            }
        }
    }

 private:
    int32_t n_;
};

class EmittingThing : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init) {
        auto clsName = Nan::New("EmitterThing").ToLocalChecked();
        auto constructor = Nan::New<v8::FunctionTemplate>(New);
        auto tpl = constructor->InstanceTemplate();
        constructor->SetClassName(clsName);
        tpl->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(constructor, "on", On);
        Nan::SetPrototypeMethod(constructor, "run", Run);
        Nan::SetPrototypeMethod(constructor, "runReentrant", RunReentrant);

        Nan::Set(target, clsName, Nan::GetFunction(constructor).ToLocalChecked());
    };

 private:
    EmittingThing() : emitter_(std::make_shared<EventEmitter>()) {}
    static NAN_METHOD(On) {
        if (info.Length() != 2) {
            info.GetIsolate()->ThrowException(Nan::TypeError("Wrong number of arguments"));
            return;
        }
        if (!info[0]->IsString()) {
            info.GetIsolate()->ThrowException(Nan::TypeError("First argument must be string"));
            return;
        }
        if (!info[1]->IsFunction()) {
            info.GetIsolate()->ThrowException(Nan::TypeError("Second argument must be function"));
            return;
        }

        auto s = std::string(*v8::String::Utf8Value(info[0]->ToString()));
        Nan::Callback* callback = new Nan::Callback(info[1].As<Function>());

        auto thing = Nan::ObjectWrap::Unwrap<EmittingThing>(info.Holder());
        thing->emitter_->on(s, callback);
    }

    static NAN_METHOD(Run) {
        if (info.Length() != 1) {
            info.GetIsolate()->ThrowException(Nan::TypeError("Wrong number of arguments"));
            return;
        }
        if (!info[0]->IsNumber()) {
            info.GetIsolate()->ThrowException(Nan::TypeError("First argument must be number"));
            return;
        }

        int32_t n = info[0]->Int32Value();
        auto thing = Nan::ObjectWrap::Unwrap<EmittingThing>(info.Holder());

        TestWorker* worker = new TestWorker(nullptr, thing->emitter_, n);
        Nan::AsyncQueueWorker(worker);
    }

    static NAN_METHOD(RunReentrant) {
        if (info.Length() != 1) {
            info.GetIsolate()->ThrowException(Nan::TypeError("Wrong number of arguments"));
            return;
        }
        if (!info[0]->IsNumber()) {
            info.GetIsolate()->ThrowException(Nan::TypeError("First argument must be number"));
            return;
        }

        int32_t n = info[0]->Int32Value();
        auto thing = Nan::ObjectWrap::Unwrap<EmittingThing>(info.Holder());

        TestReentrantWorker* worker = new TestReentrantWorker(nullptr, thing->emitter_, n);
        Nan::AsyncQueueWorker(worker);
    }

    static NAN_METHOD(New) {
        if (!info.IsConstructCall()) {
            info.GetIsolate()->ThrowException(Nan::TypeError("call to constructor without keyword new"));
            return;
        }

        EmittingThing* o = new EmittingThing();
        o->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    std::shared_ptr<NodeEvent::EventEmitter> emitter_;
};

NAN_MODULE_INIT(InitAll) { EmittingThing::Init(target); }
NODE_MODULE(NanObject, InitAll);
