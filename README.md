Node.js native EventEmitter. Allows you to implement Native C++ code that
behaves like an EventEmitter. Users of your native module will be able to write
code like 

```javascript
foo = new YourNativeExtension();
foo.on("some event", function(value) {
	console.log(value);
})
```

And inside your native code, you will be able to emit events as they happen,
which will trigger the appropriate callbacks in javascript. The emit occurs in
a separate thread from your AsyncWorker, and so will not appreciably block the
work being done.

Examples of using The EventEmitter, the first is the non-reentrant
non-threadsafe version, the second is the threadsafe and reentrant, but
requires you pass the emitter object around

```c++

class TestWorker : public AsyncEventEmittingCWorker<1024> {
 public:
    TestWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter, size_t n)
        : AsyncEventEmittingCWorker(callback, emitter), n_(n) {}

    virtual void ExecuteWithEmitter(eventemitter_fn emitter) override {
        for (int32_t i = 0; i < n_; ++i) {
            stringstream ss;
            ss << "Test" << i;
            emitter("test", ss.str().c_str());
            emitter("test2", ss.str().c_str());
            emitter("test3", ss.str().c_str());
        }
    }

 private:
    int32_t n_;
};

class TestReentrantWorker : public AsyncEventEmittingReentrantCWorker<1024> {
 public:
    TestReentrantWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter, size_t n)
        : AsyncEventEmittingReentrantCWorker(callback, emitter), n_(n) {}

    virtual void ExecuteWithEmitter(const ExecutionProgressSender* sender, eventemitter_fn_r emitter) override {
        for (int32_t i = 0; i < n_; ++i) {
            stringstream ss;
            ss << "Test" << i;
            emitter((void*)sender, "test", ss.str().c_str());
            emitter((void*)sender, "test2", ss.str().c_str());
            emitter((void*)sender, "test3", ss.str().c_str());
        }
    }

 private:
    int32_t n_;
};
```

And then for the object that behaves like an EventEmitter, you add the 'on'
method to register callbacks on events, and have some sort of "run" methods
that do asynchronous work

```c++

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

```

