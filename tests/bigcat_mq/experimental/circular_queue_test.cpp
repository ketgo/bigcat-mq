/**
 * Copyright 2022 Ketan Goyal
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <unordered_set>

#include <gtest/gtest.h>

#include <bigcat_mq/details/experimental/circular_queue.hpp>

#include "utils/chrono.hpp"
#include "utils/threads.hpp"

using namespace bigcat::details::experimental;

class CircularQueueTestFixture : public ::testing::Test {
 protected:
  static constexpr auto kBufferSize = 100;
  static constexpr auto kMaxProducers = 5;
  static constexpr auto kMaxConsumers = 5;

  using Queue = CircularQueue<char, kBufferSize, kMaxProducers, kMaxConsumers>;
  Queue queue_;

 public:
  // Publish data to ring buffer
  void Publish(const std::string& data,
               std::chrono::microseconds delay = std::chrono::microseconds{0}) {
    do {
      std::this_thread::sleep_for(delay);
    } while (queue_.Publish(data) != CircularQueueResult::SUCCESS);
  }

  // Consume data from ring buffer
  void Consume(std::string& data,
               std::chrono::microseconds delay = std::chrono::microseconds{
                   0}) const {
    Queue::ReadSpan span;

    do {
      std::this_thread::sleep_for(delay);
    } while (queue_.Consume(span) != CircularQueueResult::SUCCESS);
    data = std::string(span.Data(), span.Size());
  }
};

template <class T, class U>
void Print(const U* start, const size_t size, const char sep = ',',
           const char end = '\n') {
  const auto len = size * sizeof(U) / sizeof(T);
  for (size_t i = 0; i < len; ++i) {
    std::cout << *(reinterpret_cast<const T*>(start) + i) << sep;
  }
  std::cout << end;
}

TEST_F(CircularQueueTestFixture, TestPublishConsumeSingleThread) {
  constexpr auto kMessageCount = 3;

  std::unordered_set<std::string> write_data;
  for (size_t i = 0; i < kMessageCount; ++i) {
    std::string data = "testing_" + std::to_string(i);
    ASSERT_EQ(queue_.Publish(data), CircularQueueResult::SUCCESS);
    write_data.insert(data);
  }

  for (size_t i = 0; i < kMessageCount; ++i) {
    Queue::ReadSpan span;
    ASSERT_EQ(queue_.Consume(span), CircularQueueResult::SUCCESS);
    std::string data(span.Data(), span.Size());
    ASSERT_TRUE(write_data.find(data) != write_data.end());
  }
}

TEST_F(CircularQueueTestFixture,
       TestPublishMultipleThreadsConsumeSingleThread) {
  constexpr auto kThreadCount = 5;

  std::array<std::string, kThreadCount> write_data;
  std::array<std::string, kThreadCount> read_data;
  for (size_t i = 0; i < kThreadCount; ++i) {
    write_data[i] = "testing_" + std::to_string(i);
  }
  utils::RandomDelayGenerator<> rand(1, 5);

  {
    utils::Threads producers(kThreadCount);
    for (size_t i = 0; i < kThreadCount; ++i) {
      producers[i] = std::thread(&CircularQueueTestFixture::Publish, this,
                                 std::ref(write_data[i]), rand());
    }

    for (size_t i = 0; i < kThreadCount; ++i) {
      Consume(read_data[i]);
    }
  }

  // TODO: ASSERT consumed data
  for (size_t i = 0; i < kThreadCount; ++i) {
    std::cout << read_data[i] << "\n";
  }
}

TEST_F(CircularQueueTestFixture, TestPublishConsumeMultipleThreads) {
  constexpr auto kProducers = 5;
  constexpr auto kConsumers = 5;

  std::array<std::string, kProducers> write_data;
  std::array<std::string, kConsumers> read_data;
  for (size_t i = 0; i < kProducers; ++i) {
    write_data[i] = "testing_" + std::to_string(i);
  }
  utils::RandomDelayGenerator<> rand(1, 5);

  {
    utils::Threads producers(kProducers);
    utils::Threads consumers(kConsumers);
    for (size_t i = 0; i < kProducers; ++i) {
      producers[i] = std::thread(&CircularQueueTestFixture::Publish, this,
                                 std::ref(write_data[i]), rand());
    }
    for (size_t i = 0; i < kConsumers; ++i) {
      consumers[i] = std::thread(&CircularQueueTestFixture::Consume, this,
                                 std::ref(read_data[i]), rand());
    }
  }

  // TODO: ASSERT consumed data
  for (size_t i = 0; i < kConsumers; ++i) {
    std::cout << read_data[i] << "\n";
  }
}
