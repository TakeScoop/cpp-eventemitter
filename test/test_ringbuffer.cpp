#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "../shared_ringbuffer.hpp"

using namespace std;

// std::atomic<bool> write_done;
// std::atomic<bool> read_done;

TEST_CASE("Verify enqueue and dequeue non-blocking") {
  RingBuffer<std::string, 2> buf;

  // Enqueue 2 elements
  bool success = buf.push("Test 1");
  REQUIRE(true == success);

  success = buf.push("Test 2");
  REQUIRE(true == success);

  // Queue is full, should return false
  success = buf.push("Test 3");
  REQUIRE(false == success);

  std::string out;
  // Dequeue 2 elements
  success = buf.pop(out);
  REQUIRE("Test 1" == out);
  REQUIRE(true == success);

  success = buf.pop(out);
  REQUIRE(true == success);
  REQUIRE("Test 2" == out);

  // Queue is empty, should return false
  success = buf.pop(out);
  REQUIRE(false == success);
}

#ifndef HAVE_BOOST
TEST_CASE("Test that the writer blocks correctly when ringbuffer is full") {
  std::atomic<bool> write_done{false};
  std::atomic<bool> next{false};

  std::mutex lock;
  RingBuffer<std::string, 2> buf;

  std::unique_lock<std::mutex> guard{lock};
  std::condition_variable cond;
  auto writer = std::thread([&write_done, &buf, &next, &cond]() {
    buf.push_blocking("Test 1"); // doesn't block
    buf.push_blocking("Test 2"); // doesn't block
    next.store(true, std::memory_order_release);
    cond.notify_one();

    buf.push_blocking("Test 3"); // blocks
    write_done.store(true, std::memory_order_release);
    cond.notify_one();
  });

  auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();

  auto timer = std::thread([&cond]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cond.notify_one();
  });
  timer.detach();

  while (!(next.load(std::memory_order_acquire)) &&
         (now - start < std::chrono::seconds(1))) {
    cond.wait(guard);
    now = std::chrono::steady_clock::now();
  }
  REQUIRE(false == write_done.load());

  start = std::chrono::steady_clock::now();
  now = std::chrono::steady_clock::now();

  timer = std::thread([&cond]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cond.notify_one();
  });
  timer.detach();

  std::string out;
  bool success = buf.pop(out);
  REQUIRE(true == success);
  REQUIRE("Test 1" == out);

  while (!(write_done.load(std::memory_order_acquire)) &&
         (now - start < std::chrono::seconds(1))) {
    cond.wait(guard);
    now = std::chrono::steady_clock::now();
  }
  REQUIRE(true == write_done.load());
  writer.join();
}

TEST_CASE("Test that the reader blocks correctly when ringbuffer is empty") {
  std::atomic<bool> read_done{false};
  std::atomic<bool> next{false};
  std::mutex lock;
  RingBuffer<std::string, 2> buf;

  std::unique_lock<std::mutex> guard{lock};
  std::condition_variable cond;
  auto reader = std::thread([&read_done, &buf, &cond]() {
    string v = buf.pop_blocking();
    REQUIRE("Test 1" == v);
    read_done.store(true, std::memory_order_acq_rel);
    cond.notify_one();
  });

  // Make sure we give the reader time to block on something
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  buf.push("Test 1");

  auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();

  auto timer = std::thread([&cond]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    cond.notify_one();
  });
  timer.detach();

  while (!(read_done.load(std::memory_order_acq_rel)) &&
         (now - start < std::chrono::seconds(1))) {
    cond.wait(guard);
    now = std::chrono::steady_clock::now();
  }
  REQUIRE(true == read_done.load());
  reader.join();
}

typedef size_t TestType;
#define enqueue(VALUE)                                                         \
  do {                                                                         \
    auto _t = new TestType[1];                                                 \
    _t[0] = VALUE;                                                             \
    buf.push_blocking(std::move(_t));                                          \
  } while (0)
#define dequeue(VALUE)                                                         \
  do {                                                                         \
    auto _t = buf.pop_blocking();                                              \
    REQUIRE(_t[0] == VALUE);                                                   \
    delete[] _t;                                                               \
  } while (0)

TEST_CASE("Test that the ringbuffer cycles") {
  RingBuffer<TestType *, 4> buf;

  for (size_t i = 0; i < 50; ++i) {
    enqueue(i);
    dequeue(i);
  }
}
// TODO: this sometimes segfault, investigate that!!!
TEST_CASE("Test multi-producer/multi-consumer") {
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  producers.resize(10);
  consumers.resize(10);

  RingBuffer<TestType *, 128> buf;
  std::atomic<bool> producer_stop{false};
  std::atomic<bool> consumer_stop{false};
  std::array<size_t, 10> producer_bucket_counts{{0}};
  std::array<size_t, 10> consumer_bucket_counts{{0}};

  for (size_t n = 0; n < 10; ++n) {
    producers.emplace_back(
        [n, &producer_bucket_counts, &buf, &producer_stop]() {
          while (!producer_stop.load()) {
            enqueue(1);
            ++producer_bucket_counts[n];
          }
        });

    consumers.emplace_back(
        [n, &consumer_bucket_counts, &buf, &consumer_stop]() {
          while (!consumer_stop.load()) {
            while (buf.read_available()) {
              size_t *m;
              if (buf.pop(m)) {
                delete m;
                ++consumer_bucket_counts[n];
              }
            }
          }
        });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  producer_stop = true;
  for (auto &p : producers) {
    if (p.joinable()) {
      p.join();
    }
  }
  consumer_stop = true;
  for (auto &c : consumers) {
    if (c.joinable()) {
      c.join();
    }
  }
  size_t producer_total = 0;
  for (size_t p : producer_bucket_counts) {
    REQUIRE(p > 0);
    producer_total += p;
  }
  size_t consumer_total = 0;
  for (size_t c : consumer_bucket_counts) {
    REQUIRE(c > 0);
    consumer_total += c;
  }
  REQUIRE(consumer_total == producer_total);
}

#endif
