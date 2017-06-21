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
a seperate thread from your AsyncWorker, and so will not appreciably block the
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

